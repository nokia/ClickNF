/*
 * sslserver.{cc,hh} -- SSL encryption/decryption server
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
#if HAVE_OPENSSL
# include <openssl/err.h>
#endif
#include <click/tcpanno.hh>
#include "elements/tcp/tcpinfo.hh"
#include "sslserver.hh"
CLICK_DECLS

#if HAVE_OPENSSL
SSLServer::SSLServer() : _verbose(false)
{
}

int
SSLServer::configure(Vector<String> &conf, ErrorHandler *errh)
{
	// Self-signed certificate parameters
	_c  = "CountryName";
	_st = "StateOrProvinceName";
	_l  = "Locality";
	_o  = "Organization";
	_ou = "OrganizationalUnit";
	_cn = "CommonName";

	if (Args(conf, this, errh)
	    .read("CERT_FILE", _cert_file)
	    .read("PKEY_FILE", _pkey_file)
	    .read("C", _c)
	    .read("ST", _st)
	    .read("L", _l)
	    .read("O", _o)
	    .read("OU", _ou)
	    .read("CN", _cn)
	    .complete() < 0)
		return -1;

    return 0;
}

int
SSLServer::initialize(ErrorHandler *errh)
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

	// Generate a key or load it from file
	EVP_PKEY *pkey;

	if (_pkey_file.empty()) {
		EVP_PKEY_CTX * pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
		if (!pctx)
			return errh->error("error allocating key context");
		if (EVP_PKEY_keygen_init(pctx) <= 0)
			return errh->error("error intializing key");
		if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 1024) <= 0)
			return errh->error("could not set key bits");
		if (EVP_PKEY_keygen(pctx, &pkey) <= 0)
			return errh->error("could not generate key");

		SSL_CTX_use_PrivateKey(_ctx, pkey);

		EVP_PKEY_CTX_free(pctx);
	}
	else {
		const char *f = _pkey_file.c_str();
		if (SSL_CTX_use_PrivateKey_file(_ctx, f, SSL_FILETYPE_PEM) != 1) {
			const char *e = ERR_reason_error_string(ERR_get_error());
			return errh->error("error loading pkey file %s: %s", f, e);
		}
		pkey = SSL_CTX_get0_privatekey(_ctx);
	}

	// Generate self-signed certificate or load it from file
	if (_cert_file.empty()) {
		X509 *x509 = X509_new();
		if (!x509)
			return errh->error("could not allocate certificate");

		if (X509_set_version(x509, 2) != 1)
			return errh->error("error setting certificate version");

		if (ASN1_INTEGER_set(X509_get_serialNumber(x509), 0) != 1)
			return errh->error("error setting certificate serial number");

		if (!X509_gmtime_adj(X509_get_notBefore(x509), 0) ||
		    !X509_gmtime_adj(X509_get_notAfter(x509), (long)60*60*24*365))
			return errh->error("error setting certificate time");

		if (!X509_set_pubkey(x509, pkey))
			return errh->error("error setting certificate public key");

		X509_NAME *name = X509_get_subject_name(x509);
		
		// Sometimes it does not work... Why?!
// 		if (!X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
// 		                         (const unsigned char *)_c.c_str(),  -1, -1, 0))
// 			return errh->error("error setting certificate subject (C)");

		if (!X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC,
		                         (const unsigned char *)_st.c_str(), -1, -1, 0))
			return errh->error("error setting certificate subject (ST)");

		if (!X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC,
		                         (const unsigned char *)_l.c_str(),  -1, -1, 0))
			return errh->error("error setting certificate subject (L)");

		if (!X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
		                         (const unsigned char *)_o.c_str(),  -1, -1, 0))
			return errh->error("error setting certificate subject (O)");

		if (!X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC,
		                         (const unsigned char *)_ou.c_str(), -1, -1, 0))
			return errh->error("error setting certificate subject (OU)");

		if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
		                         (const unsigned char *)_cn.c_str(), -1, -1, 0))
			return errh->error("error setting certificate subject (CN)");

		if (X509_set_issuer_name(x509, name) != 1)
			return errh->error("error setting certificate issuer name");

		if (X509_set_subject_name(x509, name) != 1)
			return errh->error("error setting certificate subject name");

		if (!X509_sign(x509, pkey, EVP_md5()))
			return errh->error("error signing certificate");

		if (SSL_CTX_use_certificate(_ctx, x509) != 1)
			return errh->error("error loading certificate to SSL context");

		X509_free(x509);
	}
	else {
		const char *f = _cert_file.c_str();
		if (SSL_CTX_use_certificate_file(_ctx, f, SSL_FILETYPE_PEM) != 1) {
			const char *e = ERR_reason_error_string(ERR_get_error());
			return errh->error("error loading cert file %s: %s", f, e);
		}
	}

	// Check that the private key and certificate match
	if (SSL_CTX_check_private_key(_ctx) != 1) {
		const char *e = ERR_reason_error_string(ERR_get_error());
		return errh->error("certificate and key mismatch: %s", e);
	}

	// Generate temporary RSA key
	RSA *rsa = RSA_generate_key(512, RSA_F4, NULL, NULL);
	SSL_CTX_set_tmp_rsa(_ctx, rsa);
	RSA_free(rsa);

	_nthreads = master()->nthreads();
	_thread = new ThreadData[_nthreads];
	
	for (uint32_t c = 0; c < _nthreads; c++) {
		// Resize socket table
		_thread[c]._socket.resize(TCPInfo::usr_capacity());
	}

	return 0;
}

void
SSLServer::cleanup(CleanupStage)
{
	for (uint32_t c = 0; c < _nthreads; c++) {
		for (int i = 0; i < _thread[c]._socket.capacity(); i++) {
			SSLSocket *s = &(_thread[c]._socket[i]);

			if (s->ssl) {
				SSL_shutdown(s->ssl);
				SSL_free(s->ssl);
				s->clear();
			}
		}
	}
}

void
SSLServer::push(int port, Packet *p)
{
	int sockfd = TCP_SOCKFD_ANNO(p);
	
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];
	
	assert(sockfd < t->_socket.capacity());

	// Get SSL socket information
	SSLSocket *s = &(t->_socket[sockfd]);

	// Process network and application packets
	switch (port) {
	case SSL_SERVER__IN_NET_PORT:
		// If new connection, create SSL socket
		if (!s->ssl && TCP_SOCK_ADD_FLAG_ANNO(p)) {
			// Make sure this is a new connection
			assert(!s->ssl);

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
			SSL_set_accept_state(s->ssl);
			
			if (_verbose)
				click_chatter("%s: Accepting SSL connection on sockfd %d", class_name(), sockfd);
		}

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
			
			if (_verbose)
				click_chatter("%s: Connection cloded by peer on sockfd %d", class_name(), sockfd);

			// Notify application
			output(SSL_SERVER_OUT_APP_PORT).push(p);
			return;
		}

		// Empty packet
		if (!p->length()) {
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
				click_chatter("%s: bad BIO_write()", class_name());
				SSL_shutdown(s->ssl);
			}

			break;
		}

		// If handshake is not finished yet, call do_hanshake
		if (!SSL_is_init_finished(s->ssl))
			SSL_do_handshake(s->ssl);

		if (_verbose)
			click_chatter("%s: SSL Handshake finished on sockfd %d", class_name(), sockfd);
		
		break;

	case SSL_SERVER__IN_APP_PORT:
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
			if (TCP_SOCK_ADD_FLAG_ANNO(p) || TCP_SOCK_DEL_FLAG_ANNO(p))
				output(SSL_SERVER_OUT_NET_PORT).push(p);
			else
				p->kill();
			return;
		}

		// Insert packet into TX queue
		s->txq.push_back(p);

		// If SSL handshake is not finished, try again later
		if (!SSL_is_init_finished(s->ssl)){
			if (_verbose)
				click_chatter("%s: SSL Handshake on sockfd %d not finished yet", class_name(), sockfd);
			break;
		}

		// If handshake is over and there are packets in the queue, send them
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

		break;

	default:
		assert(0);
	}

	if (s->txq.empty() && s->shutdown){
		if (_verbose)
			click_chatter("%s: shutting down sockfd %d", class_name(), sockfd);
		SSL_shutdown(s->ssl);
	}

	// Read cleartext and send it to the application
	while (SSL_pending(s->ssl) || BIO_ctrl_pending(s->wbio)) {
		Packet *k = Packet::make((const void *)NULL, 0);
		if (k) {
			WritablePacket *q = k->uniqueify();
			assert(q);
			int numRd = SSL_read(s->ssl, q->data(), q->tailroom());
   			if (numRd > 0) {
				q = q->put(numRd);
				SET_TCP_SOCKFD_ANNO(q, sockfd);
				output(SSL_SERVER_OUT_APP_PORT).push(q);
			}
			else {
				q->kill();
				break;
			}
		}
	}

	// Read encrypted text and send it to the network
	while (BIO_ctrl_pending(s->rbio)) {
		Packet *k = Packet::make((const void *)NULL, 0);
		if (k) {
			WritablePacket *q = k->uniqueify();
			assert(q);
			int numRd = BIO_read(s->rbio, q->data(), q->tailroom());
			if (numRd > 0) {
				q = q->put(numRd);
				SET_TCP_SOCKFD_ANNO(q, sockfd);
				output(SSL_SERVER_OUT_NET_PORT).push(q);
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

		output(SSL_SERVER_OUT_NET_PORT).push(q);
	}
}

#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SSLServer)
