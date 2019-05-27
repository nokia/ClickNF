/*
 * bbrtcptransmit.{cc,hh} -- sets packet annotation before transmission
 * Myriana Rifai
 *
 * Copyright (c) 2018 Nokia Bell Labs
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
#include <click/tcpanno.hh>
#include "bbrtcptransmit.hh"
#include "../tcpstate.hh"
CLICK_DECLS

BBRTCPTransmit::BBRTCPTransmit() {
}

Packet *
BBRTCPTransmit::smaction(Packet *p) {
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s && !s->txq.empty() && !s->rtxq.empty());
	/**
	 * If there are packets already in flight, then we need to start
	 * delivery rate samples from the time we received the most recent ACK,
	 * to try to ensure that we include the full time the network needs to
	 * deliver all in-flight packets.  If there are no packets in flight
	 * yet, then we can start the delivery rate interval at the current
	 * time, since we know that any ACKs after now indicate that the network
	 * was able to deliver those packets completely in the sampling interval
	 * between now and the next ACK.
	 *
	 */
	if (!s->txq.empty() || !s->rtxq.empty()) {
		uint64_t tstamp_us = (uint64_t) Timestamp::now_steady().usecval();
		s->first_sent_time = tstamp_us;
		s->delivered_ustamp = tstamp_us;
	}
	click_assert(s && p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(ip && th);
	if (s->state == TCP_ESTABLISHED) {
		uint32_t seq = TCP_SEQ(th);
		uint32_t end = TCP_END(ip, th);
		pkt_state *p_s = new pkt_state(seq, end, s->delivered,
				s->first_sent_time, s->delivered_ustamp, s->app_limited, NULL,
				NULL);
		s->rs->pkt_states.push_back(p_s);
		s->bbr->handle_restart_from_idle(s);
	}
	return p;
}

void BBRTCPTransmit::push(int, Packet *p) {
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
BBRTCPTransmit::pull(int) {
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BBRTCPTransmit)
ELEMENT_MT_SAFE(BBRTCPTransmit)
