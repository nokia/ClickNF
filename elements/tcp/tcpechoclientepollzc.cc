/*
 * tcpechoclientepollzc.{cc,hh} -- a zero-copy echo client using epoll_wait()
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
#include <click/master.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <iostream>
#include "tcpechoclientepollzc.hh"
#include "util.hh"
CLICK_DECLS

TCPEchoClientEpollZC::TCPEchoClientEpollZC()
	: _thread(0), _end_h(0), _verbose(0)
{
}

int
TCPEchoClientEpollZC::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_length = 64;
	_parallel = 1;
	_connections = 1;
	bool stop = true;

	if (Args(conf, this, errh)
		.read_mp("ADDRESS", _addr)
		.read_mp("PORT", _port)
		.read("STOP", stop)
		.read("LENGTH", _length)
		.read("CONNECTIONS", _connections)
		.read("PARALLEL", _parallel)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

    if (_length > 1448)
		return errh->error("LENGTH must be less than or equal to 1448");

	if (_connections == 0)
		return errh->error("CONNECTIONS must be positive");

	if (_parallel == 0)
		return errh->error("PARALLEL must be positive");

	if (stop)
		_end_h = new HandlerCall("stop");

	return 0;
}

int
TCPEchoClientEpollZC::initialize(ErrorHandler *errh)
{
	int r = TCPApplication::initialize(errh);
	if (r < 0)
		return r;

	// Stop after reaching the connection threshold
	if (_end_h && _end_h->initialize_write(this, errh) < 0)
		return -1;

	// Get the number of threads
	_nthreads = master()->nthreads();

	// Allocate thread data
	_thread = new ThreadData[_nthreads];
	click_assert(_thread);        
	
	//Useful to synchronize multiple clients //TODO synchronize through socket?
	click_chatter("Press Enter to start the experiment:");
        std::cin.get();
        click_chatter("Experiment started");
	
	
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
TCPEchoClientEpollZC::run_task(Task *)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];

	// Create epoll file descriptor
	t->epfd = click_epoll_create(1);
	assert(t->epfd > 0);

	// Create concurrent sockets and initiate TCP handshake
	for (uint32_t i = 0; i < MIN(_parallel, _connections); i++) {
		// Socket
		int sockfd = click_socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (sockfd < 0) {
			perror("socket");
			return false;
		}

		// Setsockopt
		struct linger lin = { .l_onoff = 1, .l_linger = 0 };
		int s = sizeof(struct linger);
		if (click_setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (void*)(&lin), s)) {
			perror("setsockopt");
			return false;
		}

		// Connect
		int err = click_connect(sockfd, _addr, _port);
		if (err == -1 && errno != EINPROGRESS) {
			perror("connect");
			return false;
		}

		// Add sockfd to the list of watched file descriptors
		struct epoll_event ev;
		ev.events = EPOLLOUT | EPOLLIN;
		ev.data.fd = sockfd;
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
			perror("epoll_ctl");
			return false;
		}

		// Increment open connection counter
		t->conn_o++;
	}

	int maxevents = 4096;
	struct epoll_event events[maxevents];

	_begin = Timestamp::now_steady();
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
        if (home_thread()->stop_flag() || t->conn_c >= _connections)
			break;
	}
	_end = Timestamp::now_steady();

	click_epoll_close(t->epfd);

	// Give other tasks a chance to run
	Timestamp second = Timestamp::make_sec(1);
	t->task->yield_timeout(second, false);

	if (_end_h)
		(void)_end_h->call_write();

	return false;
}


void
TCPEchoClientEpollZC::cleanup(CleanupStage)
{
	uint64_t conn = 0;
	for (uint32_t c = 0; c < _nthreads; c++) {
		ThreadData *t = &_thread[c];
		if (t) {
			conn += t->conn_c;
			delete t->task;
		}
	}

	double time = (_end - _begin).doubleval();
	double rate_cps = conn/time;
	click_chatter("%s: conn %llu, time %.6f, rate %.0f conn/sec",
	                                        class_name(), conn, time, rate_cps);

	delete [] _thread;

	if (_end_h)
		delete _end_h;
}

void
TCPEchoClientEpollZC::selected(int sockfd, int revents)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];

	// Check if connection is established
	if (revents & EPOLLOUT) {
		if (_verbose)
			click_chatter("%s: core %d, EPOLLOUT on sockfd %d",
			                                           class_name(), c, sockfd);

		// Once the connection is established, only wait for incoming packets  
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = sockfd;
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_MOD, sockfd, &ev) < 0) {
			perror("epoll_ctl");
			return;
		}

		// Create packet
		Packet *p = Packet::make(TCP_HEADROOM, NULL, _length, 0);
		if (!p) {
			errno = ENOMEM;
			perror("send");

			if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
				perror("epoll_ctl");

			click_close(sockfd);
			return;
		}

		// Send packet
		click_push(sockfd, p);
		if (errno) {
			perror("send");
			p->kill();

			if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
				perror("epoll_ctl");

			click_close(sockfd);
			return;
		}
	}

	if (revents & EPOLLIN) {
		if (_verbose)
			click_chatter("%s: core %d, EPOLLIN on sockfd %d",
			                                           class_name(), c, sockfd);
		// Receive packet
		Packet *p = click_pull(sockfd, 1);
		if (!p) {
			perror("pull");
			return;
		}

		// Check message size
		if (p->length() != _length) {
			click_chatter("message length %d != %d", p->length(), _length);
			p->kill();
			return;
		}

		// Kill received packet
		p->kill();

		// Remove sockfd from the list of watched file descriptors
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0) {
			perror("epoll_ctl");
			return;
		}

		// Close connection
		click_close(sockfd);

		// Increment closed connection counter
		t->conn_c++;

		// Check for connection threshold
		if (t->conn_o >= _connections)
			return;

		// Create another socket
		sockfd = click_socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (sockfd < 0) {
			perror("socket");
			return;
		}

		// Setsockopt
		struct linger lin = { .l_onoff = 1, .l_linger = 0 };
		int s = sizeof(struct linger);
		if (click_setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (void*)(&lin), s)) {
			perror("setsockopt");
			return;
		}

		// Connect
		int err = click_connect(sockfd, _addr, _port);
		if (err == -1 && errno != EINPROGRESS) {
			perror("connect");
			return;
		}

		// Add sockfd to the list of watched file descriptors
		struct epoll_event ev;
		ev.events = EPOLLOUT | EPOLLIN;
		ev.data.fd = sockfd;
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
			perror("epoll_ctl");
			return;
		}

		// Increment open connection counter
		t->conn_o++;
	}

	// Check for errors
	if (revents & (EPOLLERR|EPOLLHUP)) {
		if (_verbose)
			click_chatter("%s: core %d, EPOLLERR|EPOLLHUP on sockfd %d", 
			                                           class_name(), c, sockfd);

		// Remove sockfd from the list of watched file descriptors
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
			perror("epoll_ctl");

		// Close connection
		click_close(sockfd);

		// Increment closed connection counter
		t->conn_c++;

		// Check for connection threshold
		if (t->conn_o >= _connections)
			return;

		// Create another socket
		sockfd = click_socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (sockfd < 0) {
			perror("socket");
			return;
		}

		// Setsockopt
		struct linger lin = { .l_onoff = 1, .l_linger = 0 };
		int s = sizeof(struct linger);
		if (click_setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (void*)(&lin), s)) {
			perror("setsockopt");
			return;
		}

		// Connect
		int err = click_connect(sockfd, _addr, _port);
		if (err == -1 && errno != EINPROGRESS) {
			perror("connect");
			return;
		}

		// Add sockfd to the list of watched file descriptors
		struct epoll_event ev;
		ev.events = EPOLLOUT | EPOLLIN;
		ev.data.fd = sockfd;
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
			perror("epoll_ctl");
			return;
		}

		// Increment open connection counter
		t->conn_o++;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPEchoClientEpollZC)
ELEMENT_REQUIRES(TCPApplication)
