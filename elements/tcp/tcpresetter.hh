/*
 * tcpresetter.{cc,hh} -- sends a RST for the TCP connection
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


#ifndef CLICK_TCPRESETTER_HH
#define CLICK_TCPRESETTER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPResetter

=s tcp

creates and sends a TCP RST packet based on the incoming packet

=d

A RST packet is generated from the incoming packet. If the RST is from an
ongoing flow (i.e., TCP state annotation is set in the incoming packet), then
SEQ = RCV.NXT, which is read from the connection state. If the incoming packet
does not belong to an ongoing flow, then the fields of the outgoing RST are
filled depending on the ACK flag of the incoming packet. If the ACK flag is set, then SEQ = SEG.ACK and CTL = RST. Otherwise, if the ACK flag is off, then 
SEQ = 0, ACK = SEG.SEQ + SEG.LEN, and CTL=RST,ACK.

Different than other elements (e.g., TCPAckEncap, TCPFinEncap, TCPSynEncap),
TCPResetter also includes the IP header in the outgoing packet, since it is
possible that a RST may have to be sent withouth an existing connection.

=a TCPAckEncap, TCPFinEncap, TCPSynEncap */

class TCPResetter final : public Element { public:

	TCPResetter() CLICK_COLD;

	const char *class_name() const { return "TCPResetter"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

};

CLICK_ENDDECLS
#endif
