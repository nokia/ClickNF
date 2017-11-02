/*
 * tcpgetseqanno.{cc,hh} -- reads annotation and sets the TCP sequence number
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

#ifndef CLICK_TCPGETSEQANNO_HH
#define CLICK_TCPGETSEQANNO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPGetSeqAnno

=s tcp

sets the TCP sequence number to the packet SEQ annotation

=d

Incoming packets are expected to have a TCP header in the beginning. The TCP
sequence number is set to the packet SEQ annotation. This is useful in
retransmission packets, where we need to replace the TCP headers to update the
TCP timestamp, SACK information, window value, and ACK number. 

The TCPSetSeqAnno element can be used to set the SEQ annotation to the packet TCP sequence number.

=e

The IP and TCP headers can be replaced as follows:  

    ... -> StripIPHeader
        -> TCPSetSeqAnno
        -> StripTCPHeader
        -> TCPAckOptionsEncap
        -> TCPAckEncap
        -> TCPGetSeqAnno
        -> TCPSegmentation
        -> TCPIPEncap
        -> ...

=a TCPSetSeqAnno */

class TCPGetSeqAnno final : public Element { public:

	TCPGetSeqAnno() CLICK_COLD;

	const char *class_name() const  { return "TCPGetSeqAnno"; }
	const char *port_count() const  { return PORTS_1_1; }
	const char *processing() const  { return AGNOSTIC; }
	
	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

};

CLICK_ENDDECLS
#endif
