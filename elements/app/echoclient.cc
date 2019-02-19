/*
 * echoserver.{cc,hh} -- a protocol-independent echo server
 * Massimo Gallo
 *
 * Copyright (c) 2019 Nokia Bell Labs
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
#include "echoclient.hh"
CLICK_DECLS

EchoClient::EchoClient() : _verbose(false)
{
}

int
EchoClient::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
		.read_mp("ADDRESS", _addr)
		.read_mp("PORT", _port)
		.read("LENGTH", _length)
		.read("CONNECTIONS", _connections)
		.read("PARALLEL", _parallel)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	return 0;
}

int
EchoClient::initialize(ErrorHandler *errh)
{
	int r = TCPApplication::initialize(errh);
	if (r < 0)
		return r;

	// Get the number of threads
	_nthreads = master()->nthreads();   
	
	// Allocate thread data
	_thread = new ThreadData[_nthreads];
	click_assert(_thread);        
	
	//Useful to synchronize multiple clients 
	click_chatter("Press Enter to start the experiment:");
        std::cin.get();
        click_chatter("Experiment started");
	
	// Start per-core tasks
	for (uint32_t c = 0; c < _nthreads; c++) {
		Task *t = new Task(this);
		ScheduleInfo::initialize_task(this, t, errh);
		t->move_thread(c);	
	}
	
	return 0;
}

bool
EchoClient::run_task(Task *)
{
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];
	
	t->begin = Timestamp::now_steady();
	// Create concurrent sockets and initiate TCP handshake
	for (uint32_t i = 0; i < MIN(_parallel, _connections); i++){
		t->conn_c=0;
		t->conn_o=0;
		new_connection();
	}
	return true;
}

void
EchoClient::push(int, Packet *p)
{
	int fd = TCP_SOCKFD_ANNO(p);
	unsigned c = click_current_cpu_id();
	ThreadData *t = &_thread[c];
	
	//WARNING!!! Do not double check existence of fd. 
	if (TCP_SOCK_OUT_FLAG_ANNO(p)){
		if (_verbose)
			click_chatter("%s: New connection established throuh sockfd %d",class_name(), fd);
		p->kill();
		
		// Create packet
		Packet *q = Packet::make(TCP_HEADROOM, NULL, _length, 0);
		SET_TCP_SOCKFD_ANNO(q, fd);
		output(0).push(q);
		return;
	}
	
	if (TCP_SOCK_DEL_FLAG_ANNO(p)) {
		//TODO re-establish connection
		if (_verbose)
			click_chatter("Error, connection closed by server");
		return;
	}
	
	if (!p->length()) 
		p->kill();
	
	//Send empty message to close connection (using TCPEpollServer)
	p->reset();
	SET_TCP_SOCK_DEL_FLAG_ANNO(p);
	SET_TCP_SOCKFD_ANNO(p, fd);
	output(0).push(p);

	// Increment closed connection counter
	t->conn_c++;

	// Check for connection threshold
	if (t->conn_c == _connections){
		t->end = Timestamp::now_steady();

		double time = (t->end - t->begin).doubleval();
		double rate_cps = t->conn_c/time;
		click_chatter("%s: core %d conn %llu, time %.6f, rate %.0f conn/sec",
							class_name(), c, t->conn_c, time, rate_cps);
		return;
	}
	
	new_connection();
	
	t->conn_o++;
	
	return;
}

void
EchoClient::new_connection()
{
	//Create a Socket and signal it to the epoll client -
	int sockfd = click_socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
	assert(sockfd > 0);
	
	// Set LINGER option
	struct linger lin = { .l_onoff = 1, .l_linger = 0 };
	assert(click_setsockopt(sockfd, SOL_SOCKET, SO_LINGER,(void*) (&lin),sizeof(lin))==0);

	//Send an empty message to open connection (using TCPEpollClient)
	Packet* q = Packet::make((const void *)NULL, 0);
	assert(q);
	SET_TCP_SOCK_ADD_FLAG_ANNO(q);
	SET_TCP_SOCKFD_ANNO(q, sockfd);
	SET_TCP_DPORT_ANNO(q,_port);
	q->set_dst_ip_anno(_addr);
	
	if (_verbose)
		click_chatter("%s: Creating new socket to connect to %s:%d", class_name(), _addr.unparse().c_str(), _port);
	output(0).push(q);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EchoClient)
