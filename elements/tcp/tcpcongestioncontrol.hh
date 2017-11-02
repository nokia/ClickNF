/*
 * tcpcongestioncontrol.{cc,hh} -- congestion control based on RFCs 5681/6582
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
#ifndef CLICK_TCPCONGESTIONCONTROL_HH
#define CLICK_TCPCONGESTIONCONTROL_HH

#include <click/element.hh>
#include <click/string.hh>
#include "tcpstate.hh"
CLICK_DECLS

#define TCP_CCO_IN_SYN_PORT  0  // Received SYN or SYN-ACK
#define TCP_CCO_IN_ACK_PORT  1  // Received new ACK
#define TCP_CCO_IN_OLD_PORT  2  // Received old ACK
#define TCP_CCO_IN_RTX_PORT  3  // Retransmissions

#define TCP_CCO_OUT_SYN_PORT 0
#define TCP_CCO_OUT_ACK_PORT 1
#define TCP_CCO_OUT_RTX_PORT 2
#define TCP_CCO_OUT_DAT_PORT 3

class TCPCongestionControl final : public Element { public:

	TCPCongestionControl() CLICK_COLD;

	const char *class_name() const { return "TCPCongestionControl"; }
	const char *port_count() const { return "4/4"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *) final;
	
  private:

	void handle_syn(Packet *p);
	void handle_ack(Packet *p);
	void handle_old(Packet *p);
	void handle_rtx(Packet *p);

	String unparse(TCPState *) const;

	bool _verbose;
};


CLICK_ENDDECLS
#endif

