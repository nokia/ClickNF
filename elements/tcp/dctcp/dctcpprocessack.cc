/*
 * tcpprocessack.{cc,hh} -- Process TCP ACK flag
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
#include "dctcpprocessack.hh"
#include "../tcptimers.hh"
#include "../tcpstate.hh"
#include "../tcpinfo.hh"
#include "../util.hh"
CLICK_DECLS

DCTCPProcessAck::DCTCPProcessAck()
{
}

Packet *
DCTCPProcessAck::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);

	// RFC 793:
	// "fifth check the ACK field
	//    if the ACK bit is off drop the segment and return"
	if (unlikely(!(th->th_flags & TH_ACK))) {
		p->kill();
		return NULL;
	}

	// RFC 8257
	//   1.  If the CE codepoint is set and DCTCP.CE is false, set DCTCP.CE to
	//       true and send an immediate ACK.
	//   2.  If the CE codepoint is not set and DCTCP.CE is true, set DCTCP.CE
	//       to false and send an immediate ACK.
	//   3.  Otherwise, ignore the CE codepoint.
	if ((((ip->ip_tos & IP_ECNMASK) == IP_ECN_CE) && ! s->ce)
			|| (((ip->ip_tos & IP_ECNMASK) != IP_ECN_CE) && s->ce)) {
		s->ce = ! s->ce;
		SET_TCP_ACK_FLAG_ANNO(p);
		SET_TCP_ECE_FLAG_ANNO(p);
	}
	// Reset annotation for number of bytes acked
	SET_TCP_ACKED_ANNO(p, 0);

	// Get sequence and acknowledgment numbers
	uint32_t seq = TCP_SEQ(th);
	uint32_t ack = TCP_ACK(th);

	// Get packet timestamp
	Timestamp now = p->timestamp_anno();
				
	//   "if the ACK bit is on"
	switch (s->state) {
	case TCP_SYN_RECV:

		// "If SND.UNA =< SEG.ACK =< SND.NXT then enter ESTABLISHED state
		//  and continue processing.
		//
		//  If the segment acknowledgment is not acceptable, form a
		//  reset segment,
		//
		//    <SEQ=SEG.ACK><CTL=RST>
		//
		//  and send it.
		if (!s->is_acceptable_ack(ack)) {
//			s->lock.release();
			SET_TCP_STATE_ANNO(p, 0);
			output(DCTCP_PROCESS_ACK_OUT_RTR).push(p);
			return NULL;
		}

		s->state = TCP_ESTABLISHED;
		if (s->snd_reinitialize_timer)
			s->snd_rto = 3 * TCP_RTO_INIT;

		// RFC 1122:
		// "(f)  Check ACK field, SYN-RECEIVED state, p. 72: When the
		//       connection enters ESTABLISHED state, the variables
		//       listed in (c) must be set."
		s->snd_wnd = (TCP_WIN(th) << s->snd_wscale);
		s->snd_wl1 = seq;
		s->snd_wl2 = ack;
		s->snd_wnd_max = MAX(s->snd_wnd, s->snd_wnd_max);

#if HAVE_TCP_KEEPALIVE
		// Start keepalive timer
		if (now) {
			Timestamp tmo = now + Timestamp::make_msec(TCP_KEEPALIVE);
			s->keepalive_timer.schedule_at_steady(tmo);
		}
		else
			s->keepalive_timer.schedule_after_msec(TCP_KEEPALIVE);
#endif

		// If connection is passive, add pointer to the parent's accept queue
		if (s->is_passive) {
			// Get parent TCB
//			IPFlowID flow = s->flow;
//			flow.set_daddr(IPAddress());
//			flow.set_dport(0);
//			TCPState *t = TCPInfo::flow_lookup(flow);
			TCPState *t = s->parent;

			// If parent is gone, reset connection and remove flow from table
			if (!t) {
				// Send RST
				output(DCTCP_PROCESS_ACK_OUT_RST).push(p);

				// Remove it from the flow table
				TCPInfo::flow_remove(s);

				// Unlock socket state
//				s->lock.release();

				// Wait for a grace period and deallocate TCB
//				synchronize_rcu();
				TCPState::deallocate(s);

				return NULL;
			}

			// Lock socket state
//			t->lock.acquire();

			// Insert it into the accept queue of the parent
//			t->acq.push_back(s);
			t->acq_push_back(s);

			// Wake up parent
			t->wake_up(TCP_WAIT_ACQ_NONEMPTY);

			// Unlock socket state
//			t->lock.release();
		}
		else
			s->wake_up(TCP_WAIT_CON_ESTABLISHED);

		// fallthrough

	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
	case TCP_CLOSE_WAIT:
	case TCP_CLOSING:

		// "If SND.UNA < SEG.ACK =< SND.NXT then, set SND.UNA <- SEG.ACK.
		//  Any segments on the retransmission queue which are thereby
		//  entirely acknowledged are removed.  Users should receive
		//  positive acknowledgments for buffers which have been SENT and
		//  fully acknowledged (i.e., SEND buffer should be returned with
		//  "ok" response).  If the ACK is a duplicate
		//  (SEG.ACK < SND.UNA), it can be ignored.  If the ACK acks
		//  something not yet sent (SEG.ACK > SND.NXT) then send an ACK,
		//  drop the segment, and return.
		//
		//  If SND.UNA < SEG.ACK =< SND.NXT, the send window should be
		//  updated.  If (SND.WL1 < SEG.SEQ or (SND.WL1 = SEG.SEQ and
		//  SND.WL2 =< SEG.ACK)), set SND.WND <- SEG.WND, set
		//  SND.WL1 <- SEG.SEQ, and set SND.WL2 <- SEG.ACK.
		//
		//  Note that SND.WND is an offset from SND.UNA, that SND.WL1
		//  records the sequence number of the last segment used to update
		//  SND.WND, and that SND.WL2 records the acknowledgment number of
		//  the last segment used to update SND.WND.  The check here
		//  prevents using old segments to update the window."
		//
		//  Corrections from RFC 112
		// "(g)  Check ACK field, ESTABLISHED state, p. 72: The ACK is a
		//  duplicate if SEG.ACK =< SND.UNA (the = was omitted).
		//  Similarly, the window should be updated if: SND.UNA =<
		//  SEG.ACK =< SND.NXT."

		// Correction from RFC 1122
		if (SEQ_LEQ(s->snd_una, ack) && SEQ_LEQ(ack, s->snd_nxt)) {
#if HAVE_TCP_KEEPALIVE
			// Restart keepalive timer
			if (s->state == TCP_ESTABLISHED || s->state == TCP_CLOSE_WAIT) {
				s->snd_keepalive_count = 0;
				s->keepalive_timer.unschedule();
				if (now) {
					Timestamp tmo = 
					           now + Timestamp::make_msec(TCP_KEEPALIVE);
					s->keepalive_timer.schedule_at_steady(tmo);
				}
				else 
					s->keepalive_timer.schedule_after_msec(TCP_KEEPALIVE);
			}
#endif
			// Update window
			if (SEQ_LT(s->snd_wl1, seq) || \
		          (s->snd_wl1 == seq && SEQ_LEQ(s->snd_wl2, ack))) {
				s->snd_wnd = (TCP_WIN(th) << s->snd_wscale);
				s->snd_wl1 = seq;
				s->snd_wl2 = ack;
				s->snd_wnd_max = MAX(s->snd_wnd, s->snd_wnd_max);
			}
		}

		// Check if ACK is acceptable
		if (s->is_acceptable_ack(ack)) {
			// Set annotation for number of bytes acked
			SET_TCP_ACKED_ANNO(p, ack - s->snd_una);

			// Remove acknowledged packets from RTX queue
			s->clean_rtx_queue(ack);

			// Reset RTX count
			s->snd_rtx_count = 0;

			// Advance window
			s->snd_una = ack;
		}
		// If not, check if ACK is a duplicate
		else if (SEQ_LEQ(ack, s->snd_una))
			return p;
		// If not, ACK acknowledges something not yet sent
		else {
			output(DCTCP_PROCESS_ACK_OUT_ACK).push(p);
			return NULL;
		}

		// Do additional work depending on state
		switch (s->state) {
		case TCP_FIN_WAIT1:
			// "In addition to the processing for the ESTABLISHED state, if
			//  our FIN is now acknowledged then enter FIN-WAIT-2 and continue
			//  processing in that state."
//			if (SEQ_LT(s->snd_fsn, ack))
			if (SEQ_LEQ(s->snd_nxt, ack))
				s->state = TCP_FIN_WAIT2;

			// fallthrough
		case TCP_FIN_WAIT2:
			// "In addition to the processing for the ESTABLISHED state, if
			//  the retransmission queue is empty, the user's CLOSE can be
			//  acknowledged ("ok") but do not delete the TCB.
			//
			// (ignored, as this is done by the wait() function)
			break;

		case TCP_CLOSE_WAIT:
			// "Do the same processing as for the ESTABLISHED state."
			break;

		case TCP_CLOSING:
			// "In addition to the processing for the ESTABLISHED state, if
			//  the ACK acknowledges our FIN then enter the TIME-WAIT state,
			//  otherwise ignore the segment."
//			if (SEQ_LT(s->snd_fsn, ack)) {
			if (SEQ_LEQ(s->snd_nxt, ack)) {
				s->state = TCP_TIME_WAIT;

				// Initialize and schedule TIME-WAIT timer overloading RTX timer
				unsigned c = click_current_cpu_id();
				s->rtx_timer.assign(TCPTimers::tw_timer_hook, s);
				s->rtx_timer.initialize(TCPTimers::element(), c);
				if (now) {
					Timestamp tmo = now + Timestamp::make_msec(TCP_MSL << 1);
					s->rtx_timer.schedule_at_steady(tmo);
				}
				else 
					s->rtx_timer.schedule_after_msec(TCP_MSL << 1);
			}
//			s->lock.release();
			p->kill();
			return NULL;

		default:
			break;
		}

		return p;

	case TCP_LAST_ACK:

		// "The only thing that can arrive in this state is an
		//  acknowledgment of our FIN.  If our FIN is now acknowledged,
		//  delete the TCB, enter the CLOSED state, and return."
//		if (SEQ_LT(s->snd_fsn, ack)) {
		if (SEQ_LEQ(s->snd_nxt, ack)) {

			// Stop timers and flush queues
			s->stop_timers();
			s->flush_queues();

			// Remove from port table
			if (!s->is_passive) {
				uint16_t port = ntohs(s->flow.sport());
				TCPInfo::port_put(s->flow.saddr(), port);
			}

			// Remove from flow table
			TCPInfo::flow_remove(s);

			// Unlock socket state
//			s->lock.release();

			// Wait for a grace period and deallocate TCB
//			synchronize_rcu();
			TCPState::deallocate(s);

			p->kill();
		}
		else {
//			s->lock.release();
			p->kill();
		}
		return NULL;

	case TCP_TIME_WAIT:

		// "The only thing that can arrive in this state is a
		//  retransmission of the remote FIN.  Acknowledge it, and restart
		//  the 2 MSL timeout."
		if (TCP_FIN(th)) {
			output(DCTCP_PROCESS_ACK_OUT_ACK).push(p);
			s->rtx_timer.unschedule();
			if (now) {
				Timestamp tmo = now + Timestamp::make_msec(TCP_MSL << 1);
				s->rtx_timer.schedule_at_steady(tmo);
			}
			else 
				s->rtx_timer.schedule_after_msec(TCP_MSL << 1);
		}
		else
			p->kill();

//		s->lock.release();
		return NULL;

	default:
		assert(0);
	}

	return p;
}

void
DCTCPProcessAck::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
DCTCPProcessAck::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DCTCPProcessAck)
