/*
 * tcpepollclient.{cc,hh} -- a TCP client using epoll_wait()
 * Massimo Gallo
 * 
 *	LAN				    ClickNF
 *		|   __________	       ________________		 ________________
 *		|  |	      |	 ---> |                |  --->  |                |
 * Server <->   |  | TCPStack |	      | TCPEpollClient |	|	App	 |
 * 		|  |__________|	 <--- |________________|  <---  |________________|
 *
 * 
 * TCPEpollClient and App communicates through metadata attached to small signalling packets or to packets with payload to deliver to the App itself or to the TCPEpollClient
 * 
 * The metadata (annotations in Click) are:
 * 	- SOCKFD_ANNO contained in all packets exchanged between App and TCPEpollClient. Indicates the file descriptor with which the App element want to interact (e.g., send data) or the the file descriptor from witch the packet (signalling or payload is coming)
 *	- SOCK_DEL_FLAG_ANNO contained in signalling packets between App <---> TCPEpollClient. Indicates that the annotated file decriptor is no longer valid (i.e., remotely disconnected) or that the App element want to terminate a connection to the remote Server associated to the annotated file descriptor. 
 * 	- SOCK_ADD_FLAG_ANNO contained in signalling packets from App ---> TCPEpollClient. Indicates the App element want to establish a connection with a remote Server.
 * 	- dst_ip_anno (legacy Click packet annotation) contained in signalling ADD packets from App ---> TCPEpollClient. Indicates the IP address to which the App elements want to connect.
 * 	- TCP_DPORT_ANNO contained in signalling ADD packets from App ---> TCPEpollClient. Indicates the IP address to which the App elements want to connect.
 * 	- SOCK_OUT_FLAG_ANNO contained in signalling packets from TCPEpollClient ---> App. Indicates the connection previously requested has been established. 
 * 
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


#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <click/hashtable.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/master.hh>
#include <click/standard/scheduleinfo.hh>
#include "tcpsocket.hh"
#include "tcpepollclient.hh"
#include "util.hh"
CLICK_DECLS

TCPEpollClient::TCPEpollClient()
	: _verbose(false), _batch(1)
{
}

int
TCPEpollClient::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
		.read_mp("ADDRESS", _addr)
		.read_mp("PORT", _port)
		.read("VERBOSE", _verbose)
		.read("BATCH", _batch)
		.read("PID", _pid)
		.complete() < 0)
		return -1;

	_batch = 1; //Batch is forced to 1. Batches no yet implemented at app level. 
	
	return 0;
}

int
TCPEpollClient::initialize(ErrorHandler *errh)
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
		_thread[c].sockTable = 	Vector<Socket>(TCP_USR_CAPACITY, Socket());
		_thread[c].task = t;
		ScheduleInfo::initialize_task(this, t, errh);
		t->move_thread(c);	
	}
	
	return 0;
}


bool
TCPEpollClient::run_task(Task *)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];
	
	// Create epollfd 
	t->epfd = click_epoll_create(1);
	if (t->epfd < 0) {
		perror("epoll_create");
		return false;
	}

	if (_verbose)
		click_chatter("%s: created epoll fd %d", class_name(), t->epfd);
		
	int MAXevents = 4096;
	struct epoll_event events[MAXevents];

	for (;;) {
		// Poll active file descriptors
		int n = click_epoll_wait(t->epfd, events, MAXevents, -1);
		if (n < 0) {
			perror("epoll");
			return false;
		}
		if (_verbose) 
			click_chatter("%s: epoll %d events", class_name(), n);

		// Go over each socket file descriptor
		for (int i = 0; i < n; i++)
			selected(events[i].data.fd, events[i].events);

		// Check if we should stop
		if (home_thread()->stop_flag())
			break;
	}

	click_epoll_close(t->epfd);

	if (_verbose)
		click_chatter("%s: closing sockfd %d", class_name(), t->lfd);
		
	click_close(t->lfd);

	return false;
}

void
TCPEpollClient::selected(int sockfd, int revents)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];	

	// Check for output events
	if (revents & EPOLLOUT) {
		//Signal connection established. 
		if (_verbose)
			click_chatter("%s: connected %d", class_name(), sockfd);
  
		Packet* p = Packet::make((const void *)NULL, 0);
		SET_TCP_SOCK_OUT_FLAG_ANNO(p);
		SET_TCP_SOCKFD_ANNO(p, sockfd);
		output(TCP_EPOLL_CLIENT_OUT_APP_PORT).push(p);
		
		// Modify sockfd from epoll, registrer 
		struct epoll_event ev;
		ev.events = EPOLLIN; //Register in for incoming events only
		ev.data.fd = sockfd;
		
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_MOD, sockfd, &ev) < 0)
			perror("epoll_ctl");
	}

	// Check for input events
	if (revents & EPOLLIN) {
		// Incoming data from socket in epoll
		if (_verbose)
			click_chatter("%s: event on sockfd = %d", class_name(), sockfd);
		//TODO Send batches instead of single packets if possible
		//Drain RX queue
		Packet *p = NULL; 
		while (p = click_pull(sockfd, _batch)){
			Packet* next=NULL;
			Packet* curr=p;
			while (curr){
				next = curr->next();
				curr->set_next(NULL);
				curr->set_prev(NULL);
				if (!p->length()) {
					p->kill();
					break;
				}
				SET_TCP_SOCKFD_ANNO(curr, sockfd);
				output(TCP_EPOLL_CLIENT_OUT_APP_PORT).push(curr);
				curr = next;
			}
		}
	}

	if (revents & EPOLLOUT) {
		if (_verbose)
			click_chatter("%s: EPOLLOUT event on sockfd = %d", class_name(), sockfd);
		while ( _thread[c].sockTable[sockfd].queue.size() ){
			Packet* f = _thread[c].sockTable[sockfd].queue.front();
			_thread[c].sockTable[sockfd].queue.pop_front();
			
			f->set_next(NULL);
			f->set_prev(NULL);
			
			click_push(sockfd,f);
			if (errno) {
				//Put back packet at front to preserve in-order delivery
				if (errno == EAGAIN){
					_thread[c].sockTable[sockfd].queue.push_front(f);
					return;
				}
				else {
					perror("push");
					// Remove sockfd from epoll
					if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
						perror("epoll_ctl");
					//Clear sockfd packet queue
					_thread[c].sockTable[sockfd].queue.clear();
					return;
				}
			}			  
		}
		if (! (_thread[c].sockTable[sockfd].queue.size()) ) {
			if (_verbose)
				click_chatter("%s: unregistering sockfd %d ofr  EPOLLOUT event", class_name(), sockfd);
			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.fd = sockfd;

			if (click_epoll_ctl(t->epfd, EPOLL_CTL_MOD, sockfd, &ev) < 0) {
				perror("epoll_ctl");
				click_close(sockfd);
				return;
			}
		}
		
	}

	// Check for errors
	if (revents & (EPOLLERR|EPOLLHUP)) {
		if (_verbose)
			click_chatter("%s: closing fd %d due to error",class_name(),sockfd);

		// Remove sockfd from epoll
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
			perror("epoll_ctl");

		// Close connection
		click_close(sockfd); //This could be left to the app

		// Notify application
		Packet *p = Packet::make((const void *)NULL, 0);
		SET_TCP_SOCKFD_ANNO(p, sockfd);
		SET_TCP_SOCK_DEL_FLAG_ANNO(p);
		output(TCP_EPOLL_CLIENT_OUT_APP_PORT).push(p);
	}
}

void
TCPEpollClient::push(int port, Packet *p)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];
	
	int sockfd = TCP_SOCKFD_ANNO(p); //socket created in the App.

	if (port != TCP_EPOLL_CLIENT_IN_APP_PORT) {
		p->kill();
		return;
	}

	if (TCP_SOCK_ADD_FLAG_ANNO(p)) {
 
		if (_verbose)
			click_chatter("%s: adding fd %d to clients",class_name(),sockfd);

		//Add new client and connect. 
		IPAddress daddr = p->dst_ip_anno();
		int dport = TCP_DPORT_ANNO(p); 
		
		//Check if socket is non-blocking. 
		int flags = click_fcntl(sockfd, F_GETFL);
		if (! (flags & (SOCK_NONBLOCK))){
			click_chatter("%s: Error, socket should always be blocking.", class_name());
			p->kill();
			return;
		}

		// Force outbound connections to use client's address. 
		if (click_setsockopt(sockfd, SOL_IP, IP_BIND_ADDRESS_NO_PORT,NULL,0)<0){
			perror("setsockopt");
			p->kill();
			return;
		}

		if (_verbose)
			click_chatter("%s: binding fd %d to source address %s",class_name(),sockfd, _addr.unparse().c_str());
		
		uint16_t sport = 0;
		if (click_bind(sockfd, _addr, sport) < 0) {
			perror("bind");
			p->clear_annotations();
			SET_TCP_SOCKFD_ANNO(p, sockfd);
			SET_TCP_SOCK_DEL_FLAG_ANNO(p);
			output(TCP_EPOLL_CLIENT_OUT_APP_PORT).push(p);
			return;
		}

		
		if (_verbose)
			click_chatter("%s: connecting fd %d to  %s",class_name(),sockfd, daddr.unparse().c_str());
		
		// Connect to remote address. 
		int err = click_connect(sockfd, daddr, dport);
		
		if (err == -1 && errno != EINPROGRESS) {			
			click_close(sockfd);
			p->clear_annotations();
			SET_TCP_SOCKFD_ANNO(p, sockfd);
			SET_TCP_SOCK_DEL_FLAG_ANNO(p);
			output(TCP_EPOLL_CLIENT_OUT_APP_PORT).push(p);
			return;
		}

		struct epoll_event ev;
		ev.events = EPOLLOUT; //Register for out (connection established) events.
		ev.data.fd = sockfd;

		if (click_epoll_ctl(t->epfd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
			perror("epoll_ctl");
			p->kill();
			return;
		}
		
		//Force the socket to be bound to Client's Blocking Task. 
		click_set_task(sockfd, t->task);

		p->kill();
		return;
	}
	if (TCP_SOCK_DEL_FLAG_ANNO(p)) {
		if (click_epoll_ctl(t->epfd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
			perror("epoll_ctl");

		click_close(sockfd);

		p->kill();
		return;
	}
	

	if (!p->length()){
		p->kill();
		return;
	}

	if (_verbose)
		click_chatter("%s: Fwd a packet of %d bytes on established cponnection pair", class_name(), p->length());
	
	if (_thread[c].sockTable[sockfd].queue.size()){
		_thread[c].sockTable[sockfd].queue.push_back(p);
		return;
	}
	
	click_push(sockfd, p);

	if (errno) {
		//Save packet and register for EPOLLOUT if not enough space in TXQ 
		if (errno == EAGAIN){
			if (_verbose)
				click_chatter("%s: registering sockfd  %d for EPOLLOUT event", class_name(),  sockfd);
			
			struct epoll_event ev;
			ev.events = EPOLLIN|EPOLLOUT;
			ev.data.fd = sockfd;

			if (click_epoll_ctl(t->epfd, EPOLL_CTL_MOD, sockfd, &ev) < 0) {
				perror("epoll_ctl");
				click_close(sockfd);
				return;
			}
			_thread[c].sockTable[sockfd].queue.push_back(p);
			return;
			
		}
		else{
			perror("push");
			p->kill();
		}
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPEpollClient)
ELEMENT_REQUIRES(TCPApplication)
