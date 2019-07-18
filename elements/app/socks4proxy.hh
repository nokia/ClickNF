/*
 * socks4proxy.{cc,hh} -- a simple implementation of modular socks proxy
 * Massimo Gallo
 *
 * Copyright (c) 2018 Nokia Bell Labs
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

#ifndef CLICK_SOCKS4PROXY_HH
#define CLICK_SOCKS4PROXY_HH
#include <click/element.hh>
#include <click/tcpanno.hh>
#include "../tcp/tcpapplication.hh"
CLICK_DECLS

#define SOCKS4PROXY_IN_SRV_PORT 0   // Port 0-in:  EpollServer -> Proxy
#define SOCKS4PROXY_OUT_SRV_PORT 0  // Port 0-out: Proxy       -> EpollServer
#define SOCKS4PROXY_IN_CLI_PORT 1   // Port 1-in:  EpollClient -> Proxy
#define SOCKS4PROXY_OUT_CLI_PORT 1  // Port 1-out: Proxy       -> EpollClient

// Connection status
enum status_t {
	S_CLOSED,
	S_LISTENING,
	S_CONNECTING,
	S_ESTABLISHED,
};

class Socks4Proxy :  public TCPApplication {  public:

	Socks4Proxy() CLICK_COLD;

	const char *class_name() const { return "Socks4Proxy"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	int initialize(ErrorHandler *) CLICK_COLD;

	void push(int, Packet *);
	
	struct Socket {
		Socket()
			: pair(-1), status(S_CLOSED) { }
		Socket(int pair_, int status_)
			: pair(pair_), status(status_) { }

		int pair;
		int status;
		Packet *p;
	};
	
	typedef	Vector<struct Socket> SocketTable;

      protected:

	void socket_remove(uint16_t core, int fd);
	void socket_insert(uint16_t core, int fd, int pair_fd, int status);
	
	Vector<SocketTable> _socketTable; 
	unsigned int _nthreads;
	bool _verbose;
};

CLICK_ENDDECLS
#endif

