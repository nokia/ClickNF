/*
 * tcpepollclient.{cc,hh} -- a TCP client using epoll_wait()
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


#ifndef CLICK_TCPEPOLLCLIENT_HH
#define CLICK_TCPEPOLLCLIENT_HH
#include <click/element.hh>
#include <click/packetqueue.hh>
#include "tcpapplication.hh"
#include "blockingtask.hh"
CLICK_DECLS

#define TCP_EPOLL_CLIENT_IN_NET_PORT 0 // Port 0: Network -> Application
#define TCP_EPOLL_CLIENT_OUT_APP_PORT 0
#define TCP_EPOLL_CLIENT_IN_APP_PORT 1 // Port 1: Application -> Network
#define TCP_EPOLL_CLIENT_OUT_NET_PORT 1

class TCPEpollClient final : public TCPApplication { public:

	TCPEpollClient() CLICK_COLD;

	const char *class_name() const { return "TCPEpollClient"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return "hh/hh"; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	int initialize(ErrorHandler *) CLICK_COLD;

	struct Socket {
		Socket() { }

		PacketQueue queue;
	};
	
	typedef	Vector<struct Socket> SocketTable;
	
	struct ThreadData {
		int epfd;
		int lfd;
		BlockingTask *task;
		SocketTable  sockTable;

		ThreadData() : epfd(-1), lfd(-1), task(NULL) { }
	} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
	
	bool run_task(Task *);
	void selected(int, int);
	void push(int, Packet *) final;

  private:

	bool _verbose;
	IPAddress _addr;
	uint16_t _port;
	uint32_t _batch;
	ThreadData *_thread;
	uint16_t _nthreads;

};

CLICK_ENDDECLS
#endif

