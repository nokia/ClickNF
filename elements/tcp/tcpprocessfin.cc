/*
 * tcpprocessfin.{cc,hh} -- Process TCP FIN flag
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
#include "tcpprocessfin.hh"
#include "tcptimers.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
CLICK_DECLS


TCPProcessFin::TCPProcessFin()
{
}

Packet *
TCPProcessFin::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);

	// RFC 793:
	// "eighth, check the FIN bit"
	//
	// "If the FIN bit is set, signal the user "connection closing" and
	//  return any pending RECEIVEs with same message, advance RCV.NXT
	//  over the FIN, and send an acknowledgment for the FIN.  Note that
	//  FIN implies PUSH for any segment text not yet delivered to the
	//  user."
	if (likely(!TCP_FIN(th)))
		return p;

	click_assert(s->rcv_nxt == TCP_END(p->ip_header(), th));

	// Adjust receive variable
	s->rcv_nxt++;

#if HAVE_TCP_DELAYED_ACK
	// Stop delayed ACK timer
	s->delayed_ack_timer.unschedule();
#endif

	// Set ACK flag
	SET_TCP_ACK_FLAG_ANNO(p);

	// Get now from packet timestamp
	Timestamp now = p->timestamp_anno();

	switch (s->state) {
	case TCP_SYN_RECV:
		// "Enter the CLOSE-WAIT state"
		s->state = TCP_CLOSE_WAIT;

		if (!s->is_passive)
			s->notify_error(ECONNRESET);
		else {
			// Get parent TCB
			TCPState *t = s->parent;

			// Remove it from the accept queue of the parent
			t->acq_erase(s);

			// Remove it from the flow table
			TCPInfo::flow_remove(s);

			// Wait for a grace period and deallocate TCB
			TCPState::deallocate(s);
		}
		break;

	case TCP_ESTABLISHED:
		// "Enter the CLOSE-WAIT state"
		s->state = TCP_CLOSE_WAIT;

		// Wake up task if waiting to receive data
		s->wake_up(TCP_WAIT_FIN_RECEIVED);

		break;

	case TCP_FIN_WAIT1:
		// "If our FIN has been ACKed (perhaps in this segment), then
		//  enter TIME-WAIT, start the time-wait timer, turn off the other
		//  timers; otherwise enter the CLOSING state"
		if (SEQ_LEQ(s->snd_nxt, TCP_ACK(th))) {
			s->stop_timers();
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
		else
			s->state = TCP_CLOSING;

		// Wake up task if waiting to receive data
		s->wake_up(TCP_WAIT_FIN_RECEIVED);

		break;

	case TCP_FIN_WAIT2: {
		// "Enter the TIME-WAIT state.  Start the time-wait timer, turn
		//  off the other timers."
		s->stop_timers();
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

		// Wake up task if waiting to receive data
		s->wake_up(TCP_WAIT_FIN_RECEIVED);

		break;
	}
	case TCP_CLOSE_WAIT:
		// "Remain in the CLOSE-WAIT state"
		break;

	case TCP_CLOSING:
		// "Remain in the CLOSING state"
		break;

	case TCP_LAST_ACK:
		// "Remain in the LAST-ACK state"
		break;

	case TCP_TIME_WAIT:
		// "Remain in the TIME-WAIT state.  Restart the 2 MSL time-wait
		//  timeout."
		s->rtx_timer.unschedule();

		if (now) {
			Timestamp tmo = now + Timestamp::make_msec(TCP_MSL << 1);
			s->rtx_timer.schedule_at_steady(tmo);
		}
		else 
			s->rtx_timer.schedule_after_msec(TCP_MSL << 1);
		break;

   default:
		assert(0);
	}

	return p;
}

void
TCPProcessFin::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPProcessFin::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPProcessFin)
