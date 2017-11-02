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

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpipencap.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPIPEncap::TCPIPEncap()
	: _df(false), _ttl(64), _tos(0), _id(0)
{
}

int
TCPIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
	uint8_t dscp = 0;
	uint8_t ecn = 0;

	if (Args(conf, this, errh)
		.read("DSCP", dscp)
		.read("ECN", ecn)
		.read("DF", _df)
		.read("TTL", _ttl)
		.complete() < 0)
		return -1;

	if (dscp > 63)
		return errh->error("invalid DSCP value");
	if (ecn > 3)
		return errh->error("invalid ECN value");

	_tos = (dscp << 2) | ecn;

	return 0;
}


Packet *
TCPIPEncap::smaction(Packet *q)
{
	TCPState *s = TCP_STATE_ANNO(q);
	click_assert(s);

	// Make space for IP header
	WritablePacket *p = q->push(sizeof(click_ip));
	click_assert(p);

	// Set IP header pointer
	click_ip *ip = reinterpret_cast<click_ip *>(p->data());
 	p->set_ip_header(ip, sizeof(click_ip));

	// IP header
	ip->ip_v   = 4;
	ip->ip_hl  = sizeof(click_ip) >> 2;
	ip->ip_tos = _tos;
	ip->ip_len = htons(p->length());
	ip->ip_id  = htons(_id++);
	ip->ip_off = 0;
	ip->ip_ttl = _ttl;
	ip->ip_p   = IP_PROTO_TCP;
	ip->ip_sum = 0;
	ip->ip_src = s->flow.saddr().in_addr();
	ip->ip_dst = s->flow.daddr().in_addr();

	return p;
}

void
TCPIPEncap::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPIPEncap::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPIPEncap)
