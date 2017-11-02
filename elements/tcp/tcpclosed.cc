/*
 * tcpclosed.{cc,hh} -- handles TCP state CLOSED
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

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include "tcpclosed.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPClosed::TCPClosed()
{
}

Packet *
TCPClosed::smaction(Packet *p)
{
//	TCPState *s = TCP_STATE_ANNO(p);
	const click_tcp *th = p->tcp_header();
	click_assert(th);

	// RFC 793:
	// "An incoming segment containing a RST is discarded."
	//
	// (...)
	//
	// "Do not process the FIN if the state is CLOSED, LISTEN or SYN-SENT
	//  since the SEG.SEQ cannot be validated; drop the segment and
	//  return."
	//
	if (TCP_RST(th) || TCP_FIN(th)) {
		p->kill();
		return NULL;
	}

	// "An incoming segment not containing a RST causes a RST to be sent 
	//  in response. The acknowledgment and sequence field values are 
	//  selected to make the reset sequence acceptable to the TCP that 
	//  sent the offending segment.
	//
	//  If the ACK bit is off, sequence number zero is used,
	//
	//    <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
	//
	//  If the ACK bit is on,
	//
	//    <SEQ=SEG.ACK><CTL=RST>"
	//
	SET_TCP_STATE_ANNO(p, 0);
	return p;
}

void
TCPClosed::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPClosed::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(TCPClosed)
