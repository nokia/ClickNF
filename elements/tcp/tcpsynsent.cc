/*
 * tcpsynsent.{cc,hh} -- handles TCP state SYN_SENT
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
#include "tcpsynsent.hh"
#include "tcpstate.hh"
#include "tcptimer.hh"
#include "tcpinfo.hh"
CLICK_DECLS

TCPSynSent::TCPSynSent()
{
}

Packet *
TCPSynSent::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(ip && th);

	// "Do not process the FIN if the state is CLOSED, LISTEN or SYN-SENT
	//  since the SEG.SEQ cannot be validated; drop the segment and
	//  return."
	if (unlikely(TCP_FIN(th))) {
		p->kill();
		return NULL;
	}

	// "first check the ACK bit
	//    If the ACK bit is set
 	//      If SEG.ACK =< ISS, or SEG.ACK > SND.NXT, send a reset (unless
	//      the RST bit is set, if so drop the segment and return)
	//
	//        <SEQ=SEG.ACK><CTL=RST>
	//
	//      and discard the segment.  Return.
	//
	//      If SND.UNA =< SEG.ACK =< SND.NXT then the ACK is acceptable."
	//
	if (unlikely(((th->th_flags & TH_ACK) && !s->is_acceptable_ack(p)))) {
		SET_TCP_STATE_ANNO(p, 0);
		checked_output_push(2, p);
		return NULL;
	}

	// "second check the RST bit
	//    If the RST bit is set
	//
	//      If the ACK was acceptable then signal the user "error:
	//      connection reset", drop the segment, enter CLOSED state,
	//      delete TCB, and return.  Otherwise (no ACK) drop the segment
	//      and return."
	if (unlikely(TCP_RST(th))) {
		// Stop timers and flush queues
		s->stop_timers();
		s->flush_queues();

		// Store the error code and wake user task
		s->notify_error(ECONNRESET);

		// Drop the RST packet and return
		p->kill();
		return NULL;
	}

	// "third check the security and precedence (ignored)"

	// "fourth check the SYN bit
	//    This step should be reached only if the ACK is ok, or there is
	//    no ACK, and it the segment did not contain a RST.
	//
	//    If the SYN bit is on and the security/compartment and precedence
	//    are acceptable then, RCV.NXT is set to SEG.SEQ+1, IRS is set to
	//    SEG.SEQ.  SND.UNA should be advanced to equal SEG.ACK (if there
	//    is an ACK), and any segments on the retransmission queue which
	//    are thereby acknowledged should be removed.
	//
	//    If SND.UNA > ISS (our SYN has been ACKed), change the connection
	//    state to ESTABLISHED, form an ACK segment
	//
	//      <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
	//
	//    and send it.  Data or controls which were queued for
	//    transmission may be included.  If there are other controls or
	//    text in the segment then continue processing at the sixth step
	//    below where the URG bit is checked, otherwise return.
	//
	//    Otherwise enter SYN-RECEIVED, form a SYN,ACK segment
	//
	//      <SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
	//    and send it.  If there are other controls or text in the
	//    segment, queue them for processing after the ESTABLISHED state
	//    has been reached, return.
	//
	if (likely(TCP_SYN(th))) {
		// Ignore SYN packets with data
		if (unlikely(TCP_LEN(ip, th) > 0)) {
			p->kill();
			return NULL;
		}

		// Initialize a few variables
//		s->rcv_isn = TCP_SEQ(th);
//		s->rcv_nxt = s->rcv_isn + 1;
		s->rcv_nxt = TCP_SEQ(th) + 1;
		s->rcv_wnd = TCPInfo::rmem();

		s->snd_wnd = TCP_WIN(th);
		s->snd_wl1 = TCP_SEQ(th);
		s->snd_wl2 = TCP_ACK(th);

		// Check for ACK flag
		if (likely(th->th_flags & TH_ACK)) {
			// Get acknowledgment number
			uint32_t ack = TCP_ACK(th);

			// Remove SYN packet from RTX queue
			if (s->clean_rtx_queue(ack)) {

				s->snd_rtx_count = 0;

				s->snd_una = ack;

				s->state = TCP_ESTABLISHED;

				if (s->snd_reinitialize_timer)
					s->snd_rto = 3 * TCP_RTO_INIT;

#if HAVE_TCP_KEEPALIVE
				// Start keepalive timer
				Timestamp now = p->timestamp_anno();
				if (now) {
					Timestamp tmo = now + Timestamp::make_msec(TCP_KEEPALIVE);
					s->keepalive_timer.schedule_at_steady(tmo);
				}
				else
					s->keepalive_timer.schedule_after_msec(TCP_KEEPALIVE);
#endif

				// Wake user task
				s->wake_up(TCP_WAIT_CON_ESTABLISHED);

				// Process options and send the final ACK
				return p;
			}

			// ACK did not acknowledged our SYN
			p->kill();
			return NULL;
		}

		// Simultaneous open
		s->state = TCP_SYN_RECV;

		// Stop retransmission timer
		s->rtx_timer.unschedule();

		// Delete SYN packet in retransmission queue
		s->rtxq.flush();

		// Reset retransmission timeout
		s->snd_rto = TCP_RTO_INIT;

		// Process options and send the SYN-ACK
		output(1).push(p);
		return NULL;
	}


	// "fifth, if neither of the SYN or RST bits is set then drop the
	//  segment and return.
	p->kill();
	return NULL;
}

void
TCPSynSent::push(int, Packet *p)
{
	p = smaction(p);
	if (likely(p))
		output(0).push(p);
}
 
Packet *
TCPSynSent::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPSynSent)
