/*
 * sslclient.{cc,hh} -- SSL encryption/decryption client
 * Rafael Laufer, Massimo Gallo
 *
 * Copyright (c) 2017 Nokia Bell Labs
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
 *    in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <click/config.h>
#include <click/router.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/tcpanno.hh>
#if HAVE_OPENSSL
# include <openssl/err.h>
#endif
#include "elements/tcp/tcpinfo.hh"
#include "sslclient.hh"
CLICK_DECLS

#if HAVE_OPENSSL
SSLClient::SSLClient() : _verbose(false)
{
}

int
SSLClient::configure(Vector<String> &conf, ErrorHandler *errh)
{
	// By default, reject self-signed certificates
	_self_signed = false;

	if (Args(conf, this, errh)
		.read("SELF_SIGNED", _self_signed)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

    return 0;
}

int
SSLClient::initialize(ErrorHandler *errh)
{
	int r = SSLBase::initialize(errh);
	if (r < 0)
		return r;

	// SSL method
	const SSL_METHOD *method = SSLv23_method();
	if (!method)
		return errh->error("SSLv23 method not available");

	// Allocate SSL context
	_ctx = SSL_CTX_new(method);
	if (!_ctx)
		return errh->error("error allocating SSL context");

	// Disable SSLv2
	SSL_CTX_set_options(_ctx, SSL_OP_NO_SSLv2);

	// Set default verify path (usually /usr/lib/ssl/certs)
	if (SSL_CTX_set_default_verify_paths(_ctx) != 1)
		return errh->error("error setting default verify path");

	// Cipher list
	if (!SSL_CTX_set_cipher_list(_ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"))
		return errh->error("error setting cipher list");

	// Client verifies server, but server does not verify client
	SSL_CTX_set_verify(_ctx, SSL_VERIFY_NONE, NULL);

	// Resize socket table
	_socket.resize(TCPInfo::usr_capacity());

	return 0;
}

void
SSLClient::cleanup(CleanupStage s)
{
	for (int i = 0; i < _socket.capacity(); i++) {
		SSLSocket *s = &_socket[i];

		if (s->ssl) {
			SSL_shutdown(s->ssl);
			SSL_free(s->ssl);
			s->clear();
		}
	}

	SSLBase::cleanup(s);
}

void
SSLClient::push(int port, Packet *p)
{
	int sockfd = TCP_SOCKFD_ANNO(p);
	assert(sockfd < _socket.capacity());

	// Get SSL socket information
	SSLSocket *s = &_socket[sockfd];

	// Process network and application packets
	switch (port) {
	case SSL_CLIENT__IN_NET_PORT:
		// No SSL socket
		if (!s->ssl) {
			p->kill();
			return;
		}

		// Connection closed by peer
		if (TCP_SOCK_DEL_FLAG_ANNO(p)) {
			SSL_shutdown(s->ssl);
			SSL_free(s->ssl);
			s->clear();

			// Notify application
			output(SSL_CLIENT_OUT_APP_PORT).push(p);
			return;
		}

		// Empty packet
		if (!p->length()) {
			if (TCP_SOCK_ADD_FLAG_ANNO(p) || TCP_SOCK_DEL_FLAG_ANNO(p) || TCP_SOCK_OUT_FLAG_ANNO(p))
				output(SSL_CLIENT_OUT_APP_PORT).push(p);
			else
				p->kill();
			return;
		}

		// Insert packet into RX queue
		s->rxq.push_back(p);

		while (!s->rxq.empty()) {
			Packet *q = s->rxq.front();

			int numWr = BIO_write(s->wbio, q->data(), q->length());
			int err = SSL_get_error(s->ssl, numWr);

			// No errors
			if (err == SSL_ERROR_NONE) {
				q->pull(numWr);
				if (!q->length()) {
					s->rxq.pop_front();
					q->kill();
				}
				continue;
			}

			// Check if a serious error occurred
			if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
				click_chatter("%s: bad BIO_write(), shutting down sockfd %d",
				   class_name());
				SSL_shutdown(s->ssl);
			}

			break;
		}

		// Flush written data
		(void)BIO_flush(s->wbio);
		break;

	case SSL_CLIENT__IN_APP_PORT:
		// If new connection, create SSL socket
		if (!s->ssl && TCP_SOCK_ADD_FLAG_ANNO(p)) {
			// Create SSL object
			s->ssl = SSL_new(_ctx);
			assert(s->ssl);

			// Create basic I/O (BIO) pair
			s->rbio = BIO_new(BIO_s_mem());
			s->wbio = BIO_new(BIO_s_mem());
			assert(s->rbio && s->wbio);

			// Attach BIO pair to SSL object
			SSL_set_bio(s->ssl, s->wbio, s->rbio);

			// Set behavior
			SSL_set_connect_state(s->ssl);

			// Start SSL handshake
			SSL_do_handshake(s->ssl);
			
			if (_verbose)
				click_chatter("%s: SSL Handshake started sockfd %d", class_name(), sockfd);
		}

		// No SSL socket
		if (!s->ssl) {
			p->kill();
			return;
		}

		// Check if application closed connection
		if (TCP_SOCK_DEL_FLAG_ANNO(p))
			s->shutdown = true;

		// Empty packet
		if (!p->length()) {
			if (TCP_SOCK_ADD_FLAG_ANNO(p)) //NOTE Packet with TCP_SOCK_DEL_FLAG_ANNO will be sent after SSL shutdown
				output(SSL_CLIENT_OUT_NET_PORT).push(p);
			else
				p->kill();
			return;
		}

		// Insert packet into TX queue
		s->txq.push_back(p);

		break;

	default:
		assert(0);
	}

	// Read cleartext and send it to the application
	while (SSL_pending(s->ssl) || BIO_ctrl_pending(s->wbio) > 0) {
		Packet *k = Packet::make((const void *)NULL, 0);
		if (k) {
			WritablePacket *q = k->uniqueify();
			assert(q);

			int numRd = SSL_read(s->ssl, q->data(), q->tailroom());
   			if (numRd > 0) {
				q = q->put(numRd);
				SET_TCP_SOCKFD_ANNO(q, sockfd);
				output(SSL_CLIENT_OUT_APP_PORT).push(q);
			}
			else {
				q->kill();
				break;
			}
		}
	}

	// If SSL handshake is over, verify the server certificate and trasmit data
	if (SSL_is_init_finished(s->ssl)) {
		// Check the server certificate
		if (!s->verified) {
			X509* x509 = SSL_get_peer_certificate(s->ssl);
			if (x509)
				X509_free(x509);

			int rc = SSL_get_verify_result(s->ssl);
			if ((rc == X509_V_OK) || 
			    (rc == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT && _self_signed)){
				s->verified = true;
			}
			else {
				click_chatter("%s: sockfd %d could not be verified", 
				                                          class_name(), sockfd);
				s->txq.clear();
				s->shutdown = true;
			}
		}

		// After SSL handshake and verification, send packets in TX queue
		while (!s->txq.empty()) {
			Packet *q = s->txq.front();

			int numWr = SSL_write(s->ssl, q->data(), q->length());
			int err = SSL_get_error(s->ssl, numWr);

			// No errors
			if (err == SSL_ERROR_NONE) {
				q->pull(numWr);
				if (!q->length()) {
					s->txq.pop_front();
					q->kill();
				}
				continue;
			}

			// Check if a serious error occurred
			if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
				click_chatter("%s: bad SSL_write()", class_name());
				SSL_shutdown(s->ssl);
			}

			break;
		}
	}

	// Check if we should shutdown the connection
	if (s->txq.empty() && s->shutdown) {
		if (_verbose)
			click_chatter("%s: shutting down sockfd %d", class_name(), sockfd);
		SSL_shutdown(s->ssl);
	}

	// Read encrypted text and send it to the network
	while (BIO_ctrl_pending(s->rbio) > 0) {
		Packet *k = Packet::make((const void *)NULL, 0);
		if (k) {
			WritablePacket *q = k->uniqueify();
			assert(q);

			int numRd = BIO_read(s->rbio, q->data(), q->tailroom());
			if (numRd > 0) {
				q = q->put(numRd);
				SET_TCP_SOCKFD_ANNO(q, sockfd);
				output(SSL_CLIENT_OUT_NET_PORT).push(q);
			}
			else {
				q->kill();
				break;
			}
		}
	}

	// If the connection shutdown was clean, release resources
	if (SSL_get_shutdown(s->ssl) & (SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN)) {
		if (_verbose)
			click_chatter("%s: Propagating shutdown to lower layers sockfd %d", class_name(), sockfd);

		SSL_free(s->ssl);
		s->clear();

		Packet *q = Packet::make((const void *)NULL, 0);
		SET_TCP_SOCKFD_ANNO(q, sockfd);
		SET_TCP_SOCK_DEL_FLAG_ANNO(q);

		output(SSL_CLIENT_OUT_NET_PORT).push(q);
	}
}

#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SSLClient)
