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


#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/master.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include "tcpechoserverepollzc.hh"
#include "util.hh"
CLICK_DECLS

TCPEchoServerEpollZC::TCPEchoServerEpollZC() : _thread(NULL), _verbose(0)
{
}

int
TCPEchoServerEpollZC::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_batch = 1;

	if (Args(conf, this, errh)
		.read_mp("ADDRESS", _addr)
		.read_mp("PORT", _port)
		.read("VERBOSE", _verbose)
		.read("BATCH", _batch)
		.complete() < 0)
		return -1;

	if (_batch == 0)
		return errh->error("BATCH must be positive");
	
	return 0;
}

int
TCPEchoServerEpollZC::initialize(ErrorHandler *errh)
{
	int r = TCPApplication::initialize(errh);
	if (r < 0)
		return r;

	// Get the number of threads
	_nthreads = master()->nthreads();

	// Allocate thread data
	_thread = new ThreadData[_nthreads];
	click_assert(_thread);
	
	// Start per-core tasks
	for (uint32_t c = 0; c < _nthreads; c++) {
		BlockingTask *t = new BlockingTask(this);
		_thread[c].task = t;
		ScheduleInfo::initialize_task(this, t, errh);
		t->move_thread(c);	
	}
	
	return 0;
}

bool
TCPEchoServerEpollZC::run_task(Task *)
{
	int err = 0;
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];

	// Socket
	t->lfd = click_socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (t->lfd < 0) {
		perror("socket");
		return false;
	}
	if (_verbose)
		click_chatter("%s: core %d, listen sockfd %d", class_name(), c, t->lfd);

	// Bind
	err = click_bind(t->lfd, _addr, _port);
	if (err) {
		perror("bind");
		return false;
	}
	if (_verbose)
		click_chatter("%s: core %d, bounded to %s, port %d",
		                       class_name(), c, _addr.unparse().c_str(), _port);

	// Listen
	err = click_listen(t->lfd, 4096);
	if (err) {
		perror("listen");
		return false;
	}
	if (_verbose)
		click_chatter("%s: core %d, listening at %s, port %u", 
		                       class_name(), c, _addr.unparse().c_str(), _port);

	// Create epoll file descriptor
	t->epfd = click_epoll_create(1);
	assert(t->epfd > 0);
				   
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = t->lfd;
		
	// Add listener to the list of watched file descriptors
	click_epoll_ctl(t->epfd, EPOLL_CTL_ADD, t->lfd, &ev);
	if (_verbose)
		click_chatter("%s: core %d, listener added to epoll fd %d",
		                                              class_name(), c, t->epfd);
	int maxevents = 4096;
	struct epoll_event events[maxevents];

	for (;;) {
		// Poll active file descriptors
		int n = click_epoll_wait(t->epfd, events, maxevents, -1);
		if (n < 0) {
			perror("epoll");
			return false;
		}
		if (_verbose) 
			click_chatter("%s: core %d, epoll %d events", class_name(), c, n);

		// Go over each socket file descriptor
		for (int i = 0; i < n; i++)
			selected(events[i].data.fd, events[i].events);

		// Check if we should stop
        if (home_thread()->stop_flag())
			break;
	}

	// Remove listener from the list of watched file descriptors
	click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, t->lfd, NULL);
	click_epoll_close(t->epfd);

	if (_verbose)
		click_chatter("%s: core %d, close sockfd %d", class_name(), c, t->lfd);

	click_close(t->lfd);

	return false;
}


void
TCPEchoServerEpollZC::cleanup(CleanupStage)
{
	for (uint32_t c = 0; c < _nthreads; c++)
		delete _thread[c].task;

	delete [] _thread;
}

void
TCPEchoServerEpollZC::selected(int sockfd, int revents)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];

	if (revents & EPOLLIN) {
		// If listener has a new connection, accept it
		if (sockfd == t->lfd) {
			// Accept the connection
			IPAddress addr;
			uint16_t port = 0;
			if (_verbose)
				click_chatter("%s: core %d, accept...", class_name(), c);
			
			int newfd = click_accept(t->lfd, addr, port);
			if (newfd == -1) {
				perror("accept");
				return;
			}
			if (_verbose){
				click_chatter("%s: core %d, accepted fd %d from %s port %u",
				          class_name(), c, newfd, addr.unparse().c_str(), port);
#if HAVE_DPDK
				IPFlowID flow;
				flow.assign(addr, htons(port), _addr, htons(_port));
				uint32_t hash = click_rss_hash(flow);
				uint32_t core = (hash & 127) % _nthreads;
				click_chatter("%s: core %d, flow %s goes to core %d hash %d", 
				           class_name(), c, flow.unparse().c_str(), core, hash);
#endif
			}

			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.fd = newfd;

			// Add new connection to the list of watched file descriptors
			click_epoll_ctl(t->epfd, EPOLL_CTL_ADD, newfd, &ev);
		}
		else {
			if (_verbose)
				click_chatter("%s: core %d, event on sockfd %d",
				                                       class_name(), c, sockfd);
			// Receive batch
			Packet *p = click_pull(sockfd, _batch);
			if (!p || !p->length()) {
				if (!p)
					perror("pull");

				// Remove sockfd from the list of watched file descriptors
				if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
					perror("epoll_ctl");

				// Close connection
				click_close(sockfd);
				return;
			}

			// Send batch
			click_push(sockfd, p);
			if (errno) {
				perror("send");

				// Remove sockfd from the list of watched file descriptors
				if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
					perror("epoll_ctl");

				// Close connection
				click_close(sockfd);
				return;
			}
		}
	}

	// Check for errors
	if (revents & (EPOLLERR|EPOLLHUP)) {
		if (_verbose)
			click_chatter("%s: core %d, error, closing fd %d", 
			                                           class_name(), c, sockfd);

		// Remove sockfd from the list of watched file descriptors
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
			perror("epoll_ctl");

		// Close connection
		click_close(sockfd);
		return;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPEchoServerEpollZC)
ELEMENT_REQUIRES(TCPApplication)
