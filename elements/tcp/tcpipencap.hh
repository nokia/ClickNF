/*
 * tcpipencap.{cc,hh} -- encapsulates a TCP segment with an IP header
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


#ifndef CLICK_TCPIPENCAP_HH
#define CLICK_TCPIPENCAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPIPEncap(I<KEYWORDS>)

=s ip

encapsulates packets in IP header

=d

Encapsulates each incoming packet in an IP packet with TCP protocol.

Keyword arguments are:

=over 8

=item DSCP

Number between 0 and 63. The IP header's DSCP value. Default is 0.

=item ECN

Number between 0 and 3. The IP header's ECN value. Default is 0.

=item DF

Boolean. If true, sets the IP header's DF bit to 1. Default is false.

=item TTL

Byte. The IP header's time-to-live field. Default is 64.

=back

The StripIPHeader element can be used by the receiver to get rid of the 
encapsulation header.

=e

Wraps TCP segments with an IP header. 

	... -> TCPAckOptionsEncap
	    -> TCPAckEncap
	    -> TCPIPEncap
	    -> ...

=a UDPIPEncap, StripIPHeader, TCPAckEncap, TCPFinEncap */

class TCPIPEncap final : public Element { public:

	TCPIPEncap() CLICK_COLD;

	const char *class_name() const { return "TCPIPEncap"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	bool can_live_reconfigure() const   { return true; }

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

  private:

	bool _df;
	uint8_t _ttl;
	uint8_t _tos;
	uint32_t _id;

};

CLICK_ENDDECLS
#endif
