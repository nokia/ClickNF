/*
 * tcpenqueue4rtx.{cc,hh} -- enqueue packet for retransmission
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
#include "tcpenqueue4rtx.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPEnqueue4RTX::TCPEnqueue4RTX()
{
}

int
TCPEnqueue4RTX::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_verbose = false;

	if (Args(conf, this, errh)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	return 0;
}

Packet *
TCPEnqueue4RTX::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();

	// Do not enqueue if its sequence number space (SNS) is null, e.g., pure ACK
	if (TCP_SNS(ip, th) == 0)
		return p;

	// Do not enqueue if this packet would make the RTX queue non-continuous
	if (!s->rtxq.empty() && TCP_SEQ(th) != TCP_END(s->rtxq.back()) + 1)
		return p;

	// If timestamp not supported and packet timestamp not set, get current time
	if (s->snd_ts_ok == false && p->timestamp_anno() == 0)
		p->set_timestamp_anno(Timestamp::now_steady());

	// Clone the packet to insert it into the RTX queue
	Packet *c = p->clone();

	// Print sequence space
	if (_verbose)
		click_chatter("%s: insert seq space %u:%u(%u, %u, %u)",       \
		              class_name(), TCP_SEQ(th), TCP_END(ip, th) + 1, \
		              TCP_SNS(ip, th), p->length(), ntohs(ip->ip_len));

	// Insert cloned packet into retransmission queue
	s->rtxq.push_back(c);

	// RFC 6298:
	//
	//"The following is the RECOMMENDED algorithm for managing the
	// retransmission timer:
	//
	// (5.1) Every time a packet containing data is sent (including a
	//       retransmission), if the timer is not running, start it running
	//       so that it will expire after RTO seconds (for the current value
	//       of RTO)."
	if (!s->rtx_timer.scheduled()){
		Timestamp now = p->timestamp_anno();
		if (now) {
			Timestamp tmo = now + Timestamp::make_msec(s->snd_rto);
			s->rtx_timer.schedule_at_steady(tmo);
		}
		else 
			s->rtx_timer.schedule_after_msec(s->snd_rto);
	}

	// Send out original packet
	return p;
}

void
TCPEnqueue4RTX::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPEnqueue4RTX::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPEnqueue4RTX)
