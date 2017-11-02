/*
 * tcpencap.{cc,hh} -- encapsulates packet with a TCP header
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


#ifndef CLICK_TCPENCAP_HH
#define CLICK_TCPENCAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPEncap(SRC, DST, I<KEYWORDS>)

=s ip

encapsulates packets with a TCP header

=d

Encapsulates each incoming packet with a TCP header with source port SRC and
destination port DST.

Keyword arguments are:

=over 8

=item SEQNO

Number between 0 and 63. The IP header's DSCP value. Default is 0.

=item ACKNO

Number between 0 and 3. The IP header's ECN value. Default is 0.

=item SYN

Boolean. If true, sets the TCP header's SYN bit to 1. Default is false.

=item ACK

Boolean. If true, sets the TCP header's ACK bit to 1. Default is false.

=item RST

Boolean. If true, sets the TCP header's RST bit to 1. Default is false.

=item FIN

Boolean. If true, sets the TCP header's FIN bit to 1. Default is false.

=item URG

Boolean. If true, sets the TCP header's URG bit to 1. Default is false.

=item PSH

Boolean. If true, sets the TCP header's PSH bit to 1. Default is false.

=item WINDOW

Number between 0 and 65535. The TCP header's window size. Default is 0.

=item URGENT

Number between 0 and 65535. The TCP header's urgent pointer. Default is 0.

=item TSVAL

Number between 0 and 2^32 - 1. TCP timestamp value. Default is 0. If
this is nonzero, the TCP timestamp option will be included in the header.

=item TSECR

Number between 0 and 2^32 - 1. TCP timestamp echo reply. Default is 0. If
this is nonzero, the TCP timestamp option will be included in the header.

=back

The StripTCPHeader element can be used by the receiver to get rid of the 
encapsulation header.

=e

Wraps data packets with an TCP header. 

	... -> TCPEncap
	    -> IPEncap
	    -> ...

=a UDPIPEncap, StripIPHeader, StripTCPHeader */

class TCPEncap final : public Element { public:

	TCPEncap() CLICK_COLD;

	const char *class_name() const		{ return "TCPEncap"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	bool can_live_reconfigure() const   { return true; }

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

  private:

	uint16_t _src;
	uint16_t _dst;
	uint32_t _seqno;
	uint32_t _ackno;
	uint8_t  _flags;
	uint16_t _window;
	uint16_t _urgent;
	uint32_t _tsval;
	uint32_t _tsecr;

};

CLICK_ENDDECLS
#endif
