/*
 * BBRTCPProcessAck.{cc,hh} -- Process TCP ACK flag
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "bbrtcpprocessack.hh"
#include "../tcpstate.hh"
#include "../tcpinfo.hh"
#include "../util.hh"
CLICK_DECLS

BBRTCPProcessAck::BBRTCPProcessAck() {
}

Packet *
BBRTCPProcessAck::smaction(Packet *p) {
	TCPState *s = TCP_STATE_ANNO(p);
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);
	// Get acknowledgment number
	uint32_t ack = TCP_ACK(th);
	if (s->state >= TCP_ESTABLISHED) {
		// Get RTX queue head
		pkt_state *ps = s->rs->pkt_states.front();
		while (ps) {
			// If ACK does not fully acknowledge the packet, get out
			if (unlikely(SEQ_GEQ(ps->end, ack))) {
				break;
			}
			tcp_rate_delivered(s, p, ps);
			s->delivered += ack - s->snd_una;
			rate_gen(s, (s->delivered - ps->delivered));
			s->rs->pkt_states.pop_front();
			ps = s->rs->pkt_states.front();

		}
		// Update on ack BBR states
		if (s->rs->prior_delivered > 0) {
			s->bbr->update_model_paramters_states(s);
		}
	}
	return p;
}

// method should be called when we receive the ack/sack
void BBRTCPProcessAck::tcp_rate_delivered(TCPState *s, Packet *p,
		pkt_state *ps) {

	if (!ps->delivered_time) {
		return;
	}

	if (!s->rs->prior_delivered || ps->delivered > s->rs->prior_delivered) {
		s->rs->prior_in_flight = s->tcp_packets_in_flight();
		s->rs->prior_delivered = ps->delivered;
		s->rs->prior_ustamp = ps->delivered_time;
		s->rs->is_app_limited = ps->app_limited;
		s->rs->is_retrans = s->sacked & TCPCB_RETRANS;
		//Record send time of most recently ACKed packet:
		s->first_sent_time = p->timestamp_anno().usecval();
		// Find the duration of the "send phase" of this window:
		s->rs->interval_us = s->first_sent_time - ps->first_sent_time;

	}
	/* Mark off the skb delivered once it's sacked to avoid being
	 * used again when it's cumulatively acked. For acked packets
	 * we don't need to reset since it'll be freed soon.
	 */
	if (s->sacked & TCPCB_SACKED_ACKED) {
		ps->delivered_time = (uint64_t) 0;
	}
}

void BBRTCPProcessAck::rate_gen(TCPState *s, uint32_t delivered) {

	uint32_t snd_us, ack_us;

	/* Clear app limited if bubble is acked and gone. */
	if (s->app_limited && s->delivered > s->app_limited)
		s->app_limited = 0;

	/* TODO: there are multiple places throughout tcp_ack() to get
	 * current time. Refactor the code using a new "tcp_acktag_state"
	 * to carry current time, flags, stats like "tcp_sacktag_state".
	 */
	if (delivered)
		s->delivered_ustamp = s->ts_recent_update;
	else
		return;

	s->rs->acked_sacked = delivered; /* freshly ACKed or SACKed */
	/* Return an invalid sample if no timing information is available or
	 * in recovery from loss with SACK reneging. Rate samples taken during
	 * a SACK reneging event may overestimate bw by including packets that
	 * were SACKed before the reneg.
	 */
	if (!s->rs->prior_ustamp) {
		s->rs->delivered = -1;
		s->rs->interval_us = -1;
		return;
	}
	s->rs->delivered = s->delivered - s->rs->prior_delivered;

	/* Model sending data and receiving ACKs as separate pipeline phases
	 * for a window. Usually the ACK phase is longer, but with ACK
	 * compression the send phase can be longer. To be safe we use the
	 * longer phase.
	 */
	snd_us = s->rs->interval_us; /* send phase */
	ack_us = s->ts_recent_update - s->rs->prior_ustamp; /* ack phase */
	s->rs->interval_us = std::max(snd_us, ack_us);

	/* Record both segment send and ack receive intervals */
	s->rs->snd_interval_us = snd_us;
	s->rs->rcv_interval_us = ack_us;

	/* Normally we expect interval_us >= min-rtt.
	 * Note that rate may still be over-estimated when a spuriously
	 * retransmistted skb was first (s)acked because "interval_us"
	 * is under-estimated (up to an RTT). However continuously
	 * measuring the delivery rate during loss recovery is crucial
	 * for connections suffer heavy or prolonged losses.
	 */
	if (unlikely(s->rs->interval_us < s->bbr->rtprop)) {
		s->rs->interval_us = -1;
		return;
	}

	/* Record the last non-app-limited or the highest app-limited bw */
	if (!s->rs->is_app_limited
			|| ((uint64_t) s->rs->delivered * s->rate_interval_us
					>= (uint64_t) s->rate_delivered * s->rs->interval_us)) {
		s->rate_delivered = s->rs->delivered;
		s->rate_interval_us = s->rs->interval_us;
		s->rate_app_limited = s->rs->is_app_limited;
	}
}

void BBRTCPProcessAck::push(int, Packet *p) {
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
BBRTCPProcessAck::pull(int) {
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BBRTCPProcessAck)
