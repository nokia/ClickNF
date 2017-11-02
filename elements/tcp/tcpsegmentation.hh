/*
 * tcpsegmentation.{cc,hh} -- Segment TCP packet into MTU-sized packets
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

#ifndef CLICK_TCPSEGMENTATION_HH
#define CLICK_TCPSEGMENTATION_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPSegmentation

=s tcp

segments a TCP/IP packet into multiple MSS-sized packets

=d

The incoming TCP/IP packet is segmented into several MSS-byte packets. The 
original IP and TCP header (possibly with options) is copied into each segment
and the TCP sequence number is updated. If TCP options are present, the MSS is 
reduced to not exceed the MTU. If the SYN flag is active in the original packet,
it will only be active in the first segment. Similarly, if the FIN flag is
active in the original packet, it will only be active in the last segment.

=e

Encapsulates packets with a TCP header with the ACK flag set. 

    ... -> TCPAckOptionsEncap
        -> TCPAckEncap
        -> TCPIPEncap
        -> TCPSegmentation
        -> ...

=a TCPSynEncap, TCPFinEncap, TCPAckOptionsEncap */

class TCPSegmentation final : public Element { public:

	TCPSegmentation() CLICK_COLD;

	const char *class_name() const { return "TCPSegmentation"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	void push(int, Packet *) final;

};

CLICK_ENDDECLS
#endif

