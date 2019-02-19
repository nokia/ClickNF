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

#ifndef CLICK_SSLSERVER_HH
#define CLICK_SSLSERVER_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <click/string.hh>
#include <click/packet.hh>
#include <click/packetqueue.hh>
#if HAVE_OPENSSL
# include <openssl/bio.h>
# include <openssl/ssl.h>
#endif
#include "sslbase.hh"
CLICK_DECLS


#define SSL_SERVER__IN_NET_PORT 0 // Port 0: Ciphertext -> Plaintext
#define SSL_SERVER_OUT_APP_PORT 0
#define SSL_SERVER__IN_APP_PORT 1 // Port 1: Plaintext -> Ciphertext
#define SSL_SERVER_OUT_NET_PORT 1

class SSLServer : public SSLBase { public:

    const char *class_name() const { return "SSLServer"; }
    const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }

# if HAVE_OPENSSL
    SSLServer() CLICK_COLD;

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void push(int, Packet *);
	
    struct ThreadData {
		Vector<SSLSocket> _socket;
		ThreadData() { }
    } CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);


  private:

	String _pkey_file;
	String _cert_file;
	String _c;
	String _st;
	String _l;
	String _o;
	String _ou;
	String _cn;
	SSL_CTX *_ctx;
	ThreadData *_thread;
	uint32_t _nthreads;
	bool _verbose;

# endif
};

CLICK_ENDDECLS
#endif
