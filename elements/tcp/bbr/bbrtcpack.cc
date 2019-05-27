/*
 * BBRTCPAck.{cc,hh} -- congestion avoidance
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
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/tcp.h>
#include <click/timestamp.hh>
#include <click/straccum.hh>
#include "bbrtcpack.hh"
#include "bbrstate.hh"
#include "../tcpstate.hh"
#include "../tcpinfo.hh"
#include "../util.hh"

CLICK_DECLS

BBRTCPAck::BBRTCPAck() {
}

Packet *
BBRTCPAck::smaction(Packet *p) {
	if (TCP_ACKED_ANNO(p))
		p = handle_ack(p);
	else
		p = handle_old(p);

	return p;
}

inline Packet *
BBRTCPAck::handle_ack(Packet *p) {
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	const click_tcp *th = p->tcp_header();
	uint32_t ack = TCP_ACK(th);

	// RFC 5681:
	//
	// The initial value of ssthresh SHOULD be set arbitrarily high (e.g.,
	// to the size of the largest possible advertised window), but ssthresh
	// MUST be reduced in response to congestion.
	if (s->snd_ssthresh == 0)
		s->snd_ssthresh = s->snd_wnd;

	// 6.  When the next ACK arrives that acknowledges previously
	//     unacknowledged data, a TCP MUST set cwnd to ssthresh (the value
	//     set in step 2).  This is termed "deflating" the window.
	//
	//     This ACK should be the acknowledgment elicited by the
	//     retransmission from step 3, one RTT after the retransmission
	//     (though it may arrive sooner in the presence of significant out-
	//     of-order delivery of data segments at the receiver).
	//     Additionally, this ACK should acknowledge all the intermediate
	//     segments sent between the lost segment and the receipt of the
	//     third duplicate ACK, if none of these were lost.

	// Fast recovery
	if (s->snd_dupack >= 3) {
		//   Upon every ACK in Fast Recovery, run the following
		//   BBRModulateCwndForRecovery() steps, which help ensure packet
		//   conservation on the first round of recovery, and sending at no more
		//   than twice the current delivery rate on later rounds of recovery
		//   (given that "packets_delivered" packets were newly marked ACKed or
		//   SACKed and "packets_lost" were newly marked lost):
		s->bbr->ca_state = TCP_CA_Recovery;
		//  Full acknowledgments
		if (SEQ_LT(s->snd_recover, ack)) {

			// If this ACK acknowledges all of the data up to and including
			// recover, then the ACK acknowledges all the intermediate segments
			// sent between the original transmission of the lost segment and
			// the receipt of the third duplicate ACK. Exit the fast recovery procedure.

			s->snd_dupack = 0;
			s->snd_recover = 0;
			s->snd_parack = 0;
			s->bbr->packet_conservation = false;
			//click_chatter("restore");
			//s->bbr->restore_cwnd(s);
			if (TCPInfo::verbose())
				click_chatter("%s: ack, %s, window deflate, full ACK",
						class_name(), s->unparse_cong().c_str());
		}
		// Partial acknowledgments
		else {
			// If this ACK does *not* acknowledge all of the data up to and
			// including recover, then this is a partial ACK.  In this case,
			// retransmit the first unacknowledged segment.  Deflate the
			// congestion window by the amount of new data acknowledged by the
			// Cumulative Acknowledgment field.  If the partial ACK acknowledges
			// at least one SMSS of new data, then add back SMSS bytes to the
			// congestion window.  This artificially inflates the congestion
			// window in order to reflect the additional segment that has left
			// the network.  Send a new segment if permitted by the new value of
			// cwnd.  This "partial window deflation" attempts to ensure that,
			// when fast recovery eventually ends, approximately ssthresh amount
			// of data will be outstanding in the network.  Do not exit the fast
			// recovery procedure (i.e., if any duplicate ACKs subsequently
			// arrive, execute step 4 of Section 3.2 of [RFC5681]).
			//
			// For the first partial ACK that arrives during fast recovery, also
			// reset the retransmit timer.  Timer management is discussed in
			// more detail in Section 4. 

			// Retransmit the first unacknowledged segment
			click_assert(!s->rtxq.empty());
			Packet *c = s->rtxq.front()->clone();
			click_assert(c);
			WritablePacket *wp = c->uniqueify();
			click_assert(wp);

			wp->set_next(NULL);
			wp->set_prev(NULL);

			s->bbr->ca_state = TCP_CA_Disorder;

			// Reset the retransmission timer if this is the first partial ACK
			if (s->snd_parack++ == 0) {
				s->bbr->packet_conservation = false;
				s->snd_rto = TCP_RTO_INIT;
				s->rtx_timer.unschedule();
				Timestamp now = p->timestamp_anno();
				if (now) {
					Timestamp tmo = now + Timestamp::make_msec(s->snd_rto);
					s->rtx_timer.schedule_at_steady(tmo);
				} else
					s->rtx_timer.schedule_after_msec(s->snd_rto);
			}

			if (TCPInfo::verbose())
				click_chatter("%s: ack, %s, window deflate, partial ACK",
						class_name(), s->unparse_cong().c_str());

			// Increment RTX counter
			s->snd_rtx_count++;

			// Send retransmission
			output(1).push(wp);
		}
		return p;
	}
	//s->bbr->ca_state = 0;
	// Reset dupack counter as this ACK advances the left edge of the window
	s->snd_dupack = 0;

	return p;
}

inline Packet *
BBRTCPAck::handle_old(Packet *p) {
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	// Header pointers	
	const click_tcp *th = p->tcp_header();
	const click_ip *ip = p->ip_header();

	// Get acknowledgment number, receive window, and segment length
	uint32_t ack = TCP_ACK(th);
	uint32_t win = TCP_WIN(th);
	uint16_t len = TCP_LEN(ip, th);

	// SYN and FIN flags
	bool syn = TCP_SYN(th);
	bool fin = TCP_FIN(th);

	// RFC 5681:
	//
	// The initial value of ssthresh SHOULD be set arbitrarily high (e.g.,
	// to the size of the largest possible advertised window), but ssthresh
	// MUST be reduced in response to congestion.
	if (s->snd_ssthresh == 0)
		s->snd_ssthresh = s->snd_wnd;

	//  DUPLICATE ACKNOWLEDGMENT: An acknowledgment is considered a
	//	"duplicate" in the following algorithms when (a) the receiver of
	//	the ACK has outstanding data, (b) the incoming acknowledgment
	//	carries no data, (c) the SYN and FIN bits are both off, (d) the
	//	acknowledgment number is equal to the greatest acknowledgment
	//	received on the given connection (TCP.UNA from [RFC793]) and (e)
	//	the advertised window in the incoming acknowledgment equals the
	//	advertised window in the last incoming acknowledgment.
	if (SEQ_LT(s->snd_una, s->snd_nxt) &&                 // (a)
			(len == 0) &&                 // (b)
			(!syn && !fin) &&                 // (c)
			(ack == s->snd_una) &&                 // (d)
			(win << s->snd_wscale) == s->snd_wnd)  // (e)
		s->snd_dupack++;
	else {
		s->snd_dupack = 0;
		return p;
	}

	//  The fast retransmit and fast recovery algorithms are implemented
	//  together as follows.
	switch (s->snd_dupack) {
	case 1:
	case 2:
		// 1.  On the first and second duplicate ACKs received at a sender, a
		// 	   TCP SHOULD send a segment of previously unsent data per [RFC3042]
		//     provided that the receiver's advertised window allows, the total
		//     FlightSize would remain less than or equal to cwnd plus 2*SMSS,
		//     and that new data is available for transmission.  Further, the
		//     TCP sender MUST NOT change cwnd to reflect these two segments
		//     [RFC3042].  Note that a sender using SACK [RFC2018] MUST NOT send
		//     new data unless the incoming duplicate acknowledgment contains
		//     new SACK information.
		s->bbr->ca_state = TCP_CA_Open;
		if (TCPInfo::verbose())
			click_chatter("%s: old, %s, dup ack %d, ack %u", class_name(),
					s->unparse_cong().c_str(), s->snd_dupack, ack);

		// This is implemented by default in the rate controller
		break;

	case 3: {


		s->bbr->ca_state = TCP_CA_Loss;
		// Upon entering Fast Recovery, set cwnd to the number of packets still
		//   in flight (allowing at least one for a fast retransmit):
		//s->bbr->save_cwnd(s);
		//s->snd_cwnd = (s->tcp_packets_in_flight()* s->snd_mss)
		//		+ std::max(s->bbr->delivered, (uint32_t) s->snd_mss);
		//s->bbr->packet_conservation = true;
		//click_chatter("cwnd in 3 dup %u", s->snd_cwnd);
		// Store the last sequence number transmitted when loss is detected
		click_assert(!s->rtxq.empty());
		Packet *x = s->rtxq.back();
		s->snd_recover = TCP_END(x);

		// Reset partial ACK counter
		s->snd_parack = 0;

		// Retransmit the first unacknowledged segment
		Packet *c = s->rtxq.front()->clone();
		click_assert(c);
		WritablePacket *wp = c->uniqueify();
		click_assert(wp);

		wp->set_next(NULL);
		wp->set_prev(NULL);

		// Increment RTX counter
		s->snd_rtx_count++;

		if (TCPInfo::verbose())
			click_chatter("%s: old, %s, dup ack %d, ack %u", class_name(),
					s->unparse_cong().c_str(), s->snd_dupack, ack);

		// Send retransmission
		output(1).push(wp);

		break;
	}
	default:
		break;
	}
	return p;
}

void BBRTCPAck::push(int, Packet *p) {
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
BBRTCPAck::pull(int) {
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BBRTCPAck)
