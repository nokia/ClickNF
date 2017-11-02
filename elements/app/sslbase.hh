/*
 * sslbase.{cc,hh} -- Thread-safe SSL initialization
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

#ifndef CLICK_SSLBASE_HH
#define CLICK_SSLBASE_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/packetqueue.hh>
#if HAVE_OPENSSL
# include <openssl/bio.h>
# include <openssl/ssl.h>
#endif
CLICK_DECLS

class SSLBase : public Element { public:

	const char *class_name() const { return "SSLBase"; }

# if HAVE_OPENSSL
    SSLBase() CLICK_COLD;

	int initialize(ErrorHandler *) CLICK_COLD;
	void cleanup(CleanupStage) CLICK_COLD;

	struct SSLSocket {
		SSL *ssl;
		BIO *rbio;
		BIO *wbio;
		bool verified;
		bool shutdown;
		PacketQueue txq;
		PacketQueue rxq;

		SSLSocket() : ssl(0), rbio(0), wbio(0), verified(0), shutdown(0) { }

		inline void clear() {
			ssl  = NULL;
			rbio = NULL;
			wbio = NULL;
			verified = false;
			shutdown = false;
			txq.clear();
			rxq.clear();
		}
	};
# endif
};

CLICK_ENDDECLS
#endif
