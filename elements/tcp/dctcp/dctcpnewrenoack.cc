/*
 * dctcpnewrenoack.{cc,hh} -- congestion avoidance
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
#include "dctcpnewrenoack.hh"
#include "../tcpinfo.hh"
#include "../util.hh"

#include <clicknet/ip.h>
#include <click/handlercall.hh>
CLICK_DECLS

DCTCPNewRenoAck::DCTCPNewRenoAck() {
}

Packet *
DCTCPNewRenoAck::smaction(Packet *p) {
	if (TCP_ACKED_ANNO(p))
		p = handle_ack(p);
	else
		p = handle_old(p);

	return p;
}

inline Packet *
DCTCPNewRenoAck::handle_ack(Packet *p) {
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	const click_tcp *th = p->tcp_header();
	uint32_t ack = TCP_ACK(th);
	uint32_t acked = TCP_ACKED_ANNO(p);
	click_assert(acked);

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

		//  From RFC 6582, TCP New RENO modification.
		//  There are two cases:

		//  Full acknowledgments
		if (SEQ_LT(s->snd_recover, ack)) {
			// If this ACK acknowledges all of the data up to and including
			// recover, then the ACK acknowledges all the intermediate segments
			// sent between the original transmission of the lost segment and
			// the receipt of the third duplicate ACK.  Set cwnd to either (1)
			// min (ssthresh, max(FlightSize, SMSS) + SMSS) or (2) ssthresh,
			// where ssthresh is the value set when fast retransmit was entered,
			// and where FlightSize in (1) is the amount of data presently
			// outstanding.  This is termed "deflating" the window.  If the
			// second option is selected, the implementation is encouraged to
			// take measures to avoid a possible burst of data, in case the
			// amount of data outstanding in the network is much less than the
			// new congestion window allows.  A simple mechanism is to limit the
			// number of data packets that can be sent in response to a single
			// acknowledgment.  Exit the fast recovery procedure.
			uint32_t flight = s->snd_nxt - s->snd_una;
			s->snd_cwnd = \
			         MIN(s->snd_ssthresh, MAX(flight, s->snd_mss) + s->snd_mss);
			s->snd_cwnd = MIN(s->snd_cwnd, s->snd_wnd_max);
			s->snd_dupack = 0;
			s->snd_recover = 0;
			s->snd_parack = 0;

			if (TCPInfo::verbose())
				click_chatter("%s: ack, %s, window deflate, full ACK", \
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

			// Deflate cwnd by the amount of new data acknowledged
			s->snd_cwnd -= MIN(s->snd_cwnd, acked);

			// If acknowledging at least 1 MSS, add back MSS bytes to cwnd
			if (acked >= s->snd_mss)
				s->snd_cwnd = MIN(s->snd_cwnd + s->snd_mss, s->snd_wnd_max);

			// Reset the retransmission timer if this is the first partial ACK
			if (s->snd_parack++ == 0) {
				s->snd_rto = TCP_RTO_INIT;
				s->rtx_timer.unschedule();
				Timestamp now = p->timestamp_anno();
				if (now) {
					Timestamp tmo = now + Timestamp::make_msec(s->snd_rto);
					s->rtx_timer.schedule_at_steady(tmo);
				}
				else
					s->rtx_timer.schedule_after_msec(s->snd_rto);
			}

			if (TCPInfo::verbose())
				click_chatter("%s: ack, %s, window deflate, partial ACK", \
			                           class_name(), s->unparse_cong().c_str());

			// Increment RTX counter
			s->snd_rtx_count++;

			// Send retransmission
			output(1).push(wp);
		}
		return p;
	}

	// Reset dupack counter as this ACK advances the left edge of the window
	s->snd_dupack = 0;

	// The slow start algorithm is used when cwnd < ssthresh, while the
	// congestion avoidance algorithm is used when cwnd > ssthresh.  When
	// cwnd and ssthresh are equal, the sender may use either slow start or
	// congestion avoidance.
	if (s->snd_cwnd < s->snd_ssthresh) {
		// SLOW START
		//
		// During slow start, a TCP increments cwnd by at most SMSS bytes for
		// each ACK received that cumulatively acknowledges new data.  Slow
		// start ends when cwnd exceeds ssthresh (or, optionally, when it
		// reaches it, as noted above) or when congestion is observed.  While
		// traditionally TCP implementations have increased cwnd by precisely
		// SMSS bytes upon receipt of an ACK covering new data, we RECOMMEND
		// that TCP implementations increase cwnd, per:
		//  	cwnd += min (N, SMSS)					  (2)
		// where N is the number of previously unacknowledged bytes acknowledged
		// in the incoming ACK.
		s->snd_cwnd = MIN(s->snd_cwnd + MIN(acked, s->snd_mss), s->snd_wnd_max);

		if (TCPInfo::verbose())
			click_chatter("%s: ack, %s, slow start, bytes acked %u", \
			                    class_name(), s->unparse_cong().c_str(), acked);
	}
	else {
		// CONGESTION AVOIDANCE
		//
		// During congestion avoidance, cwnd is incremented by roughly 1 full-
		// sized segment per round-trip time (RTT).  Congestion avoidance
		// continues until congestion is detected.
		//
		// (...)
		//
		// The RECOMMENDED way to increase cwnd during congestion avoidance is
		// to count the number of bytes that have been acknowledged by ACKs for
		// new data. (A drawback of this implementation is that it requires
		// maintaining an additional state variable.)  When the number of bytes
		// acknowledged reaches cwnd, then cwnd can be incremented by up to SMSS
		// bytes.
		s->snd_bytes_acked += acked;
		/**
		 * DCTCP code
		 */
		// SENDER CODE
		// RFC 8257
		// The sender estimates the fraction of bytes sent that encountered
		// congestion.  The current estimate is stored in a new TCP state
		// variable, DCTCP.Alpha, which is initialized to 1 and SHOULD be
		// updated as follows:
		//      DCTCP.Alpha = DCTCP.Alpha * (1 - g) + g * M

		s->bytes_acked += acked;
		if ((th->th_flags & TH_ECE) == TH_ECE) {
			s->bytes_marked += acked;
			if (ack > s->window_end) {
				s->alpha = s->alpha * (1 - s->gain)
						+ s->gain * (s->bytes_marked / s->bytes_acked);
				s->window_end = s->snd_nxt;
				s->bytes_acked = 0;
				s->bytes_marked = 0;
				s->snd_cwnd = s->snd_cwnd * (1 - s->alpha / 2);
				if (TCPInfo::verbose())
					click_chatter("cwnd = %u", s->snd_cwnd);
			}
		/**
		 * END DCTCP
		 */
		} else if (s->snd_bytes_acked >= s->snd_cwnd) {
			s->snd_bytes_acked -= s->snd_cwnd;
			s->snd_cwnd = MIN(s->snd_cwnd + s->snd_mss, s->snd_wnd_max);

		}

		if (TCPInfo::verbose())
			click_chatter("%s: ack, %s, cong avoid, bytes acked %u", \
			                    class_name(), s->unparse_cong().c_str(), acked);
	}
	return p;
}

inline Packet *
DCTCPNewRenoAck::handle_old(Packet *p) {
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
		return handle_ack(p);
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

		if (TCPInfo::verbose())
			click_chatter("%s: old, %s, dup ack %d, ack %u", class_name(),
					s->unparse_cong().c_str(), s->snd_dupack, ack);

		// This is implemented by default in the rate controller
		break;

	case 3: {

		// 2.  When the third duplicate ACK is received, a TCP MUST set ssthresh
		//     to no more than the value given in equation (4).  When [RFC3042]
		//     is in use, additional data sent in limited transmit MUST NOT be
		//     included in this calculation.
		//
		//          (4) ssthresh = max (FlightSize / 2, 2*SMSS)
		uint32_t mss = s->snd_mss;
		s->snd_ssthresh = MAX((s->snd_nxt - s->snd_una) >> 1, mss << 1);

		// 3.  The lost segment starting at SND.UNA MUST be retransmitted and
		//     cwnd set to ssthresh plus 3*SMSS.  This artificially "inflates"
		//     the congestion window by the number of segments (three) that have
		//     left the network and which the receiver has buffered.

		// Update congestion window
		s->snd_cwnd = MIN(s->snd_ssthresh + 3 * s->snd_mss, s->snd_wnd_max);

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
		// 4.  For each additional duplicate ACK received (after the third),
		//     cwnd MUST be incremented by SMSS.  This artificially inflates the
		//     congestion window in order to reflect the additional segment that
		//     has left the network.
		//
		//     Note: [SCWA99] discusses a receiver-based attack whereby many
		//     bogus duplicate ACKs are sent to the data sender in order to
		//     artificially inflate cwnd and cause a higher than appropriate
		//     sending rate to be used.  A TCP MAY therefore limit the number of
		//     times cwnd is artificially inflated during loss recovery to the
		//     number of outstanding segments (or, an approximation thereof).
		//
		//     Note: When an advanced loss recovery mechanism (such as outlined
		//     in section 4.3) is not in use, this increase in FlightSize can
		//     cause equation (4) to slightly inflate cwnd and ssthresh, as some
		//     of the segments between SND.UNA and SND.NXT are assumed to have
		//     left the network but are still reflected in FlightSize.
		if (s->snd_dupack <= s->rtxq.packets())
			s->snd_cwnd = MIN(s->snd_cwnd + s->snd_mss, s->snd_wnd_max);

		if (TCPInfo::verbose())
			click_chatter("%s: old, %s, dup ack %d", class_name(),
					s->unparse_cong().c_str(), s->snd_dupack);

		// 5.  When previously unsent data is available and the new value of
		//     cwnd and the receiver's advertised window allow, a TCP SHOULD
		//     send 1*SMSS bytes of previously unsent data.

		// This is implemented by default in rate controller

		// FIXME: If retransmitted packet sent when dupack = 3 is not received,
		//        then it is never retransmitted again
		break;
	}

	return p;
}

void DCTCPNewRenoAck::push(int, Packet *p) {
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
DCTCPNewRenoAck::pull(int) {
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DCTCPNewRenoAck)
