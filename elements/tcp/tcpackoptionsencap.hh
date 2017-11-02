/*
 * tcpackoptionsencap.{cc,hh} -- encapsulates packet with TCP ACK options
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


#ifndef CLICK_TCPACKOPTIONSENCAP_HH
#define CLICK_TCPACKOPTIONSENCAP_HH
#include <click/element.hh>
#include "tcpstate.hh"
CLICK_DECLS

/*
=c

TCPAckOptionsEncap

=s tcp

encapsulates packets with TCP options sent in ACK packets

=d

The TCP timestamp and/or SACK options are prepended to the packet depending on
the negotiation done at the initial TCP three-way handshake that decides the
permitted TCP options for the connection. The OPLEN annotation is set with the
size (in bytes) of the TCP options. This is required for the following element
(e.g., TCPAckEncap, TCPFinEncap) to properly set the offset in the TCP header.

The TCPAckOptionsParse element can be used by the receiver to parse the TCP
options of an incoming packet.

=e

Encapsulates data with TCP options sent in ACK packets. 

    ... -> TCPAckOptionsEncap
        -> TCPAckEncap
        -> TCPIPEncap
        -> ...

=a TCPAckEncap, TCPFinEncap, TCPSynOptionsEncap, TCPAckOptionsParse */

class TCPAckOptionsEncap final : public Element { public:

	TCPAckOptionsEncap() CLICK_COLD;

	const char *class_name() const		{ return "TCPAckOptionsEncap"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }

	static uint8_t oplen(TCPState *);
	static uint8_t min_oplen(TCPState *);
	static uint8_t max_oplen(TCPState *);

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

};

CLICK_ENDDECLS
#endif
