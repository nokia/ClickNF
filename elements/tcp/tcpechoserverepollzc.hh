/*
 * tcpechoserverepollzc.{cc,hh} -- a zero-copy echo server using epoll_wait()
 * Massimo Gallo, Rafael Laufer
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

#ifndef CLICK_TCPECHOSERVEREPOLLZC_HH
#define CLICK_TCPECHOSERVEREPOLLZC_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include "tcpapplication.hh"
#include "blockingtask.hh"
CLICK_DECLS

class TCPEchoServerEpollZC final : public TCPApplication { public:

	TCPEchoServerEpollZC() CLICK_COLD;

	const char *class_name() const { return "TCPEchoServerEpollZC"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return "h/h"; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	int initialize(ErrorHandler *) CLICK_COLD;
	void cleanup(CleanupStage) CLICK_COLD;

	bool run_task(Task *) final;
	void selected(int sockfd, int revents);

	struct ThreadData {
		int epfd;
		int lfd;
		BlockingTask *task;

		ThreadData() : epfd(-1), lfd(-1), task(NULL) { }
	} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);

  private:

	ThreadData *_thread;
	IPAddress _addr;
	uint32_t _nthreads;
	uint32_t _batch;
	uint16_t _port;
	bool _verbose;	
	
};

CLICK_ENDDECLS
#endif

