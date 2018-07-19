/*
 * tcpresetter.{cc,hh} -- reuses the incoming packet to send a RST 
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpresetter.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPResetter::TCPResetter()
{
}

Packet *
TCPResetter::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);

	// Header pointers
	click_ip *ip = const_cast<click_ip *>(p->ip_header());
	click_tcp *th = const_cast<click_tcp *>(p->tcp_header());
	click_assert(ip && th);

	// Save some state
	bool ack = (th->th_flags & TH_ACK);
	uint32_t ackno = TCP_ACK(th);
	uint32_t endno = TCP_END(ip, th);
	uint16_t sport = TCP_SRC(th);
	uint16_t dport = TCP_DST(th);
	IPAddress saddr(ip->ip_src);
	IPAddress daddr(ip->ip_dst);

	// Delete previous packet
	p->kill();

	WritablePacket *q = Packet::make(TCP_HEADROOM, NULL, 0, 0);
	q = q->push(sizeof(click_ip) + sizeof(click_tcp));
	click_assert(q);

	q->set_ip_header(reinterpret_cast<click_ip *>(q->data()), sizeof(click_ip));
	ip = q->ip_header();
	th = q->tcp_header();

	// IP header
	ip->ip_v   = 4;
	ip->ip_hl  = 5;
	ip->ip_tos = 0;
	ip->ip_len = htons(q->length());
	ip->ip_id  = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 64;
	ip->ip_p   = IP_PROTO_TCP;
	ip->ip_sum = 0;
	ip->ip_src = daddr.in_addr();
	ip->ip_dst = saddr.in_addr();

	// TCP header
	th->th_sport = ntohs(dport);
	th->th_dport = ntohs(sport);
	th->th_seq = 0;
	th->th_ack = 0;
	th->th_off = sizeof(click_tcp) >> 2;
	th->th_flags2 = 0;
	th->th_flags = TH_RST;
	th->th_win	= 0;
	th->th_sum	= 0;
	th->th_urp	= 0;

	// Fill header depending on parameters
	if (s)
		th->th_seq = htonl(s->snd_nxt);
	else if (ack)
		th->th_seq = htonl(ackno);
	else {
		th->th_ack = htonl(endno + 1);
		th->th_flags |= TH_ACK;
	}

	return q;
}

void
TCPResetter::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPResetter::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPResetter)
