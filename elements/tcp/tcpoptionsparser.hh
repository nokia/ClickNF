/*
 * tcpoptionsparser.{cc,hh} -- Parse TCP options
 * Rafael Laufer
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


#ifndef CLICK_TCPOPTIONSPARSER_HH
#define CLICK_TCPOPTIONSPARSER_HH
#include <click/element.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

#define TCP_OPP_IN_ACK_PORT  0  // Received ACK
#define TCP_OPP_IN_SYN_PORT  1  // Received SYN or SYN-ACK

#define TCP_OPP_OUT_CSN_PORT 0  // Check sequence number
#define TCP_OPP_OUT_CCO_PORT 1  // Congestion control
#define TCP_OPP_OUT_ACK_PORT 2  // Acker

class TCPOptionsParser final : public Element { public:

	TCPOptionsParser() CLICK_COLD;

	const char *class_name() const		{ return "TCPOptionsParser"; }
	const char *port_count() const		{ return "2/3"; }
	const char *processing() const		{ return "ah/ah"; }

	void push(int, Packet *) final;

  private:

	void handle_syn(Packet *p);
	void handle_ack(Packet *p);

};

CLICK_ENDDECLS
#endif
