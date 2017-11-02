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

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#if HAVE_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/crypto.h>
# if HAVE_MULTITHREAD
#  include <pthread.h>
# endif
#endif
#include "sslbase.hh"
CLICK_DECLS

#if HAVE_OPENSSL
static atomic_uint32_t initialized;


# if HAVE_MULTITHREAD
static pthread_mutex_t *mutex = NULL;

static void threadid_func(CRYPTO_THREADID *id)
{
	CRYPTO_THREADID_set_numeric(id, pthread_self());
}

static void locking_callback(int mode, int n, const char *, int)
{
	if (mode & CRYPTO_UNLOCK)
		pthread_mutex_unlock(&mutex[n]);
	else if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&mutex[n]);
}
# endif

SSLBase::SSLBase()
{
}

int
SSLBase::initialize(ErrorHandler *errh)
{
	// Ensure that SSL initialization is done only once
	if (initialized.swap(1) == 0) {
		SSL_library_init();
		OpenSSL_add_ssl_algorithms();
		OpenSSL_add_all_algorithms();
		ERR_load_crypto_strings();
		ERR_load_BIO_strings();

# if HAVE_MULTITHREAD
		int n = CRYPTO_num_locks();
		mutex = (pthread_mutex_t *)OPENSSL_malloc(n * sizeof(pthread_mutex_t));
		if (!mutex)
			return errh->error("error allocating mutex array");

		for (int i = 0; i < CRYPTO_num_locks(); i++)
			pthread_mutex_init(&mutex[i], NULL);

		CRYPTO_THREADID_set_callback(threadid_func);
		CRYPTO_set_locking_callback(locking_callback);
# endif
	}

	return 0;
}

void
SSLBase::cleanup(CleanupStage)
{
# if HAVE_MULTITHREAD
	CRYPTO_set_locking_callback(NULL);
	CRYPTO_THREADID_set_callback(NULL);

	for (int i = 0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_destroy(&mutex[i]);
# endif
}
#endif // HAVE_OPENSSL

CLICK_ENDDECLS
ELEMENT_PROVIDES(SSLBase)
