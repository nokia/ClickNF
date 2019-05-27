/*
 * tcptimers.{cc,hh} -- TCP timers
 * Rafael Laufer, Myriana Rifai
 *
 * Copyright (c) 2019 Nokia Bell Labs
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
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include "tcptimers.hh"
#include "tcpinfo.hh"
#include "util.hh"
CLICK_DECLS

TCPTimers *TCPTimers::_t = NULL;

TCPTimers::TCPTimers() {
}

int TCPTimers::configure(Vector<String> &, ErrorHandler *errh) {
	if (_t)
		return errh->error("TCPTimers can only be configured once");

	_t = this;

	return 0;
}

void TCPTimers::rtx_timer_hook(TCPTimer *t, void *data) {
	// RFC 6298:
	// "When the retransmission timer expires, do the following:
	//
	//    (5.4) Retransmit the earliest segment that has not been acknowledged
	//          by the TCP receiver.
	//
	//    (5.5) The host MUST set RTO <- RTO * 2 ("back off the timer").  The
	//          maximum value discussed in (2.5) above may be used to provide
	//          an upper bound to this doubling operation.
	//
	//    (5.6) Start the retransmission timer, such that it expires after RTO
	//          seconds (for the value of RTO after the doubling operation
	//          outlined in 5.5).
	//
	//    (5.7) If the timer expires awaiting the ACK of a SYN segment and the
	//          TCP implementation is using an RTO less than 3 seconds, the RTO
	//          MUST be re-initialized to 3 seconds when data transmission
	//          begins (i.e., after the three-way handshake completes)."
	//

	TCPState *s = reinterpret_cast<TCPState *>(data);
	click_assert(s && !s->rtxq.empty());
	// Head-of-line (HOL) packet
	Packet *q = s->rtxq.front();

	if (++s->snd_rtx_count <= TCP_RTX_MAX) {
		// RFC 2018:
		//
		// After a retransmit timeout the data sender SHOULD turn off all of the
		// SACKed bits, since the timeout might indicate that the data receiver
		// has reneged.  The data sender MUST retransmit the segment at the left
		// edge of the window after a retransmit timeout, whether or not the
		// SACKed bit is on for that segment.  A segment will not be dequeued
		// and its buffer freed until the left window edge is advanced over it.
		if (s->snd_sack_permitted) {
			Packet *p = q;
			do {
				RESET_TCP_SACK_FLAG_ANNO(p);
				p = p->next();
			} while (p != q);
		}

		// Set flag to reinitialize timer if this is a SYN retransmission
		if (TCP_SYN(q))
			s->snd_reinitialize_timer = true;

		if (TCPInfo::verbose())
			click_chatter("%s: rtx seqno %u", _t->class_name(), TCP_SEQ(q));

		// Reschedule timer
		s->snd_rto = MIN(s->snd_rto << 1, TCP_RTO_MAX);
		t->reschedule_after_msec(s->snd_rto);

		// Make a copy of the packet
		WritablePacket *p;
		Packet *c = q->clone();
		if (!c || !(p = c->uniqueify())) {
			click_chatter("%s: out of memory", _t->class_name());
			s->notify_error(ENOMEM);
			return;
		}

		// Reset next and prev pointers, as they are set at cloning 
		p->set_next(NULL);
		p->set_prev(NULL);

		// Send retransmission
		_t->output(TCP_TIMERS_OUT_RTX).push(p);
	}
	// too many retransmissions
	else {
		if (TCPInfo::verbose())
			click_chatter("%s: rtx limit reached", _t->class_name());

		s->notify_error(ETIMEDOUT);
	}
}

void TCPTimers::tx_timer_hook(TCPTimer *t, void *data) {
	TCPState *s = reinterpret_cast<TCPState *>(data);
	click_assert(s);
	// Head-of-line (HOL) packet
	Packet *q = s->bbr->pcq.front();
	if (q) {
		// Send packet
		s->bbr->pcq.pop_front();
		if (s->bbr->pacing_rate)
        	    s->next_send_time = (uint64_t) Timestamp::now_steady().usecval()
				+ (uint32_t) (q->seg_len() * 1000000 / s->bbr->pacing_rate);
	        else
	            s->next_send_time = (uint64_t) Timestamp::now_steady().usecval();
		q->set_next(NULL);
		q->set_prev(NULL);
		_t->output(TCP_TIMERS_OUT_PACING).push(q);
		t->reschedule_after_msec(
				 (uint64_t)(s->next_send_time
						- (uint64_t) Timestamp::now_steady().usecval())/1000);
	}
}

#if HAVE_TCP_KEEPALIVE
void
TCPTimers::keepalive_timer_hook(TCPTimer *t, void *data)
{
	TCPState *s = reinterpret_cast<TCPState *>(data);
	click_assert(s);

	// Make sure we are in the correct state
	if (s->state != TCP_ESTABLISHED && s->state != TCP_CLOSE_WAIT) {
		return;
	}

	if (++s->snd_keepalive_count <= TCP_KEEPALIVE_MAX) {
		if (TCPInfo::verbose())
		click_chatter("%s: keepalive timeout", _t->class_name());

		t->reschedule_after_msec(TCP_KEEPALIVE);

		WritablePacket *p = Packet::make(TCP_HEADROOM, NULL, 0, 0);
		click_assert(p);
		SET_TCP_STATE_ANNO(p, (uint64_t)s);
		_t->output(TCP_TIMERS_OUT_KAL).push(p);
	}
	// too many keepalives lost
	else {
		if (TCPInfo::verbose())
		click_chatter("%s: keepalive limit reached", _t->class_name());

		s->notify_error(ETIMEDOUT);
	}
}
#endif

#if HAVE_TCP_DELAYED_ACK
void TCPTimers::delayed_ack_timer_hook(TCPTimer *, void *data) {
	TCPState *s = reinterpret_cast<TCPState *>(data);
	click_assert(s);
//	s->lock.acquire();

	if (TCPInfo::verbose())
		click_chatter("%s: delayed ACK timeout", _t->class_name());

	Packet *p = Packet::make(TCP_HEADROOM, NULL, 0, 0);
	click_assert(p);
	SET_TCP_STATE_ANNO(p, (uint64_t )s);
	_t->output(TCP_TIMERS_OUT_ACK).push(p);
}
#endif

void TCPTimers::tw_timer_hook(TCPTimer *, void *data) {
	TCPState *s = reinterpret_cast<TCPState *>(data);
	click_assert(s);
//	s->lock.acquire();

	click_assert(s->state == TCP_TIME_WAIT && s->sockfd == -1);

	if (TCPInfo::verbose())
		click_chatter("%s: timewait timeout", _t->class_name());

	// Get source address
	IPAddress saddr = s->flow.saddr();

	// Remove from port table
	if (!s->is_passive) {
		uint16_t port = ntohs(s->flow.sport());
		TCPInfo::port_put(saddr, port);
	}

	// Remove from flow table
	TCPInfo::flow_remove(s);

	// Unlock socket state
//	s->lock.release();

	// Wait for a grace period and deallocate TCB
//	synchronize_rcu();
	TCPState::deallocate(s);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPTimers)
