/*
 * tcpsynencap.{cc,hh} -- encapsulates packet with a TCP header with SYN set
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpsynencap.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
CLICK_DECLS

TCPSynEncap::TCPSynEncap()
{
}

Packet *
TCPSynEncap::smaction(Packet *q)
{
	TCPState *s = TCP_STATE_ANNO(q);
	click_assert(s);

	// Make space for the TCP header
	WritablePacket *p = q->push(sizeof(click_tcp));
	click_assert(p);

	// Header pointers
	click_tcp *th = reinterpret_cast<click_tcp *>(p->data());

	// TCP header
	th->th_sport  = s->flow.sport();
	th->th_dport  = s->flow.dport();
	th->th_seq    = htonl(s->snd_isn);
	th->th_ack    = htonl(s->rcv_nxt);
	th->th_off    = (sizeof(click_tcp) + TCP_OPLEN_ANNO(p)) >> 2;
	th->th_flags2 = 0;
	th->th_flags  = (s->is_passive ? (TH_SYN | TH_ACK) : TH_SYN);
	th->th_win    = htons(MIN(TCPInfo::rmem(), 65535));
	th->th_sum    = 0;
	th->th_urp    = 0;

	return p;
}

void
TCPSynEncap::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPSynEncap::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPSynEncap)
