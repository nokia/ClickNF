/*
 * echoclient.{cc,hh} -- a protocol-independent echo client
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

#ifndef CLICK_ECHOCLIENT_HH
#define CLICK_ECHOCLIENT_HH
#include <click/element.hh>
#include <click/tcpanno.hh>
#include "../tcp/tcpapplication.hh"

CLICK_DECLS

class EchoClient :  public TCPApplication {  public:
  
	EchoClient() CLICK_COLD;

	const char *class_name() const { return "EchoClient"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	int initialize(ErrorHandler *) CLICK_COLD;

	void new_connection();

	struct ThreadData {
		Timestamp begin;
		Timestamp end;
		uint32_t conn_o;
		uint32_t conn_c;

		ThreadData() : conn_o(0), conn_c(0) { }
	};
	
	bool run_task(Task *) final;
	void push(int, Packet *);

  private:


	IPAddress _addr;
	int _pid;
	uint32_t _nthreads;
	uint32_t _length;
	uint32_t _connections;
	uint32_t _parallel;
	uint16_t _port;
	ThreadData *_thread;
	bool _verbose;
};

CLICK_ENDDECLS
#endif

