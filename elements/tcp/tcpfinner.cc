/*
 * tcpfinner.{cc,hh} -- sends a FIN or a FIN-ACK for the TCP connection
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpfinner.hh"
CLICK_DECLS

TCPFinner::TCPFinner()
{
}

Packet *
TCPFinner::smaction(Packet *pp)
{
	TCPState *s = TCP_STATE_ANNO(pp);
	assert(s);

	// Reuse the packet for the FIN or FIN-ACK
	WritablePacket *p = pp->uniqueify();
	assert(p);

	// Delete the data
	p->take(p->length());
	p = p->put(40);

	// Header pointers
	click_ip *ip = (click_ip *) p->data();
	click_tcp *th = (click_tcp *)(ip + 1);
 	p->set_ip_header(ip, sizeof(click_ip));

	// IP header
	ip->ip_v   = 4;
	ip->ip_hl  = 5;
	ip->ip_tos = 0;
	ip->ip_len = htons(40);
	ip->ip_id  = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 64;
	ip->ip_p   = IP_PROTO_TCP;
	ip->ip_sum = 0;
	ip->ip_src = s->flow.saddr().in_addr();
	ip->ip_dst = s->flow.daddr().in_addr();

	// TCP header
	th->th_sport  = s->flow.sport();
	th->th_dport  = s->flow.dport();
	th->th_seq    = htonl(s->snd_nxt++);
	th->th_ack    = htonl(s->rcv_nxt);
	th->th_off    = 5;
	th->th_flags2 = 0;
	th->th_flags  = TH_FIN | TH_ACK;
	th->th_win    = htons(s->rcv_wnd >> s->rcv_wscale);
	th->th_sum    = 0;
	th->th_urp    = 0;

	// Set IP destination annotation
	p->set_anno_u32(0, ip->ip_dst.s_addr);

	return p;
}

void
TCPFinner::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPFinner::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPFinner)
