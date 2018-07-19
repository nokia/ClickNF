/*
 * tcpcongestioncontrol.{cc,hh} -- congestion control based on RFCs 5681/6582
 * Massimo Gallo, Rafael Laufer
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
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/tcp.h>
#include <click/timestamp.hh>
#include <click/straccum.hh>
#include "tcpcongestioncontrol.hh"
#include "tcpstate.hh"
#include "util.hh"
CLICK_DECLS

TCPCongestionControl::TCPCongestionControl()
	: _verbose(false)
{
}

int
TCPCongestionControl::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	return 0;
}

void
TCPCongestionControl::handle_syn(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p); 
	click_assert(s);

	const click_tcp *th = p->tcp_header();
 	bool ack = (th->th_flags & TH_ACK);
	click_assert(TCP_SYN(th));

	// RFC 5681:
	//
	// IW, the initial value of cwnd, MUST be set using the following
	// guidelines as an upper bound.
	// 
	// If SMSS > 2190 bytes:
	//     IW = 2 * SMSS bytes and MUST NOT be more than 2 segments
	// If (SMSS > 1095 bytes) and (SMSS <= 2190 bytes):
	//     IW = 3 * SMSS bytes and MUST NOT be more than 3 segments
	// If SMSS <= 1095 bytes:
	//     IW = 4 * SMSS bytes and MUST NOT be more than 4 segments
	if (s->snd_mss > 2190)
		s->snd_cwnd = 2*s->snd_mss;
	else if (s->snd_mss > 1095)
		s->snd_cwnd = 3*s->snd_mss;
	else
		s->snd_cwnd = 4*s->snd_mss;

	// The initial value of ssthresh SHOULD be set arbitrarily high (e.g.,
	// to the size of the largest possible advertised window), but ssthresh
	// MUST be reduced in response to congestion.
	s->snd_ssthresh = s->snd_wnd;

	if (_verbose)
		click_chatter("%s: syn, %s", class_name(), unparse(s).c_str());
		
	// As specified in [RFC3390], the SYN/ACK and the acknowledgment of the
	// SYN/ACK MUST NOT increase the size of the congestion window.

	// If SYN-ACK, send the final ACK; otherwise, send SYN-ACK
	if (ack)
		output(TCP_CCO_OUT_ACK_PORT).push(p);
	else
		output(TCP_CCO_OUT_SYN_PORT).push(p);
}

void
TCPCongestionControl::handle_ack(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p); 
	click_assert(s);
	
 	const click_tcp *th = p->tcp_header();
	uint32_t ack = TCP_ACK(th);
	uint32_t bytes_acked = ack - s->snd_una;
	click_assert(bytes_acked > 0);

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
			s->snd_dupack = 0;
			s->snd_recover = 0;
			s->snd_parack = 0;
			
			if (_verbose)
				click_chatter("%s: ack, %s, window deflate, full ACK", \
			                                  class_name(), unparse(s).c_str());
			p->kill();
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
			assert(!s->rtxq.empty());
			WritablePacket *wp = s->rtxq.front()->clone()->uniqueify();
			click_tcp *th = wp->tcp_header();

			// Update ACK and WIN fields
			th->th_ack = htonl(s->rcv_nxt);
			th->th_win = htons(s->rcv_wnd >> s->rcv_wscale);

			// Increment RTX counter
			s->snd_rtx_count++;

			// Send retransmission
			output(TCP_CCO_OUT_RTX_PORT).push(wp);

			// Deflate cwnd by the amount of new data acknowledged
			s->snd_cwnd -= MIN(s->snd_cwnd, bytes_acked);

			// If acknowledging at least 1 MSS, add back MSS bytes to cwnd
			if (bytes_acked >= s->snd_mss)
				s->snd_cwnd += s->snd_mss;

			// Send a new segment, if window allows it
			output(TCP_CCO_OUT_DAT_PORT).push(p);

			// Reset the retransmit timer if this is the first partial ACK
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
			
			if (_verbose)
				click_chatter("%s: ack, %s, window deflate, partial ACK", \
			                                  class_name(), unparse(s).c_str());
		}
		return;
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
		s->snd_cwnd += MIN(bytes_acked, s->snd_mss);

		if (_verbose)
			click_chatter("%s: ack, %s, slow start, bytes acked %u", \
			                     class_name(), unparse(s).c_str(), bytes_acked);
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
		s->snd_bytes_acked += bytes_acked; 
		if (s->snd_bytes_acked >= s->snd_cwnd) {
			s->snd_bytes_acked -= s->snd_cwnd;
			s->snd_cwnd += s->snd_mss;
		}

		if (_verbose)
			click_chatter("%s: ack, %s, cong avoid, bytes acked %u", \
			                     class_name(), unparse(s).c_str(), bytes_acked);
	}

	p->kill();
}

void
TCPCongestionControl::handle_old(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	// Header pointers	
	const click_tcp *th = p->tcp_header();
	const click_ip *ip = p->ip_header();
	
	// Get sequence number, acknowledgment number, and segment length
	uint32_t ack = TCP_ACK(th);
	uint32_t win = TCP_WIN(th);
	uint16_t len = TCP_LEN(ip, th);

	// SYN and FIN flags
	bool syn = (th->th_flags & TH_SYN);
	bool fin = (th->th_flags & TH_FIN);
	
	// RFC 5681:
	//
	//  DUPLICATE ACKNOWLEDGMENT: An acknowledgment is considered a
	//	"duplicate" in the following algorithms when (a) the receiver of
	//	the ACK has outstanding data, (b) the incoming acknowledgment
	//	carries no data, (c) the SYN and FIN bits are both off, (d) the
	//	acknowledgment number is equal to the greatest acknowledgment
	//	received on the given connection (TCP.UNA from [RFC793]) and (e)
	//	the advertised window in the incoming acknowledgment equals the
	//	advertised window in the last incoming acknowledgment.
	if (SEQ_LT(s->snd_una, s->snd_nxt) &&                 // (a)
	               (len == 0)          &&                 // (b)
	               !(syn || fin)       &&                 // (c)
	               (ack == s->snd_una) &&                 // (d)
	               (win << s->snd_wscale) == s->snd_wnd)  // (e) 
		s->snd_dupack++;
	else
		s->snd_dupack = 0;
		  
	//  The fast retransmit and fast recovery algorithms are implemented
	//  together as follows.
	switch (s->snd_dupack) {
	case 0:
		// Kill old ACK if it is not a duplicate ACK
		p->kill();
		break;

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
		if (_verbose)
			click_chatter("%s: old, %s, dup ack %d", \
			                   class_name(), unparse(s).c_str(), s->snd_dupack);

		output(TCP_CCO_OUT_DAT_PORT).push(p);
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
		
		// TCP New Reno
		// Store the last sequence number transmitted when loss is detected
		click_assert(!s->rtxq.empty());
		Packet *x = s->rtxq.back();
		const click_ip *xip = x->ip_header();
		const click_tcp *xth = x->tcp_header();
		s->snd_recover = TCP_END(xip, xth);

		// 3.  The lost segment starting at SND.UNA MUST be retransmitted and
		//     cwnd set to ssthresh plus 3*SMSS.  This artificially "inflates"
		//     the congestion window by the number of segments (three) that have
		//     left the network and which the receiver has buffered.
		WritablePacket *wp = s->rtxq.front()->clone()->uniqueify();
		click_tcp *th = wp->tcp_header();
		
		// Increment RTX counter
		s->snd_rtx_count++;

		// Update ACK and WIN fields
		th->th_ack = htonl(s->rcv_nxt);
		th->th_win = htons(s->rcv_wnd >> s->rcv_wscale);

		if (_verbose)
			click_chatter("%s: old, %s, dup ack %d, retransmit %u", \
			      class_name(), unparse(s).c_str(), s->snd_dupack, TCP_SEQ(th));

		// Send retransmission
		output(TCP_CCO_OUT_RTX_PORT).push(wp);

		// Update congestion window
		s->snd_cwnd = s->snd_ssthresh + 3*s->snd_mss;
		
		// Kill duplicate ACK
		p->kill();

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
			s->snd_cwnd += s->snd_mss;

		if (_verbose)
			click_chatter("%s: old, %s, dup ack %d", \
			                   class_name(), unparse(s).c_str(), s->snd_dupack);

		// 5.  When previously unsent data is available and the new value of
		//     cwnd and the receiver's advertised window allow, a TCP SHOULD
		//     send 1*SMSS bytes of previously unsent data.
		output(TCP_CCO_OUT_DAT_PORT).push(p); 
		break;
	}
}

void TCPCongestionControl::handle_rtx(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);
	
	// When a TCP sender detects segment loss using the retransmission timer
	// and the given segment has not yet been resent by way of the
	// retransmission timer, the value of ssthresh MUST be set to no more
	// than the value given in equation (4):
	// 
	//	   ssthresh = max (FlightSize / 2, 2*SMSS)			(4)
	// 
	// where, as discussed above, FlightSize is the amount of outstanding
	// data in the network.
	// 
	// On the other hand, when a TCP sender detects segment loss using the
	// retransmission timer and the given segment has already been
	// retransmitted by way of the retransmission timer at least once, the
	// value of ssthresh is held constant.
	if (s->snd_rtx_count == 1) {
		uint32_t mss = s->snd_mss;
		s->snd_ssthresh = MAX((s->snd_nxt - s->snd_una) >> 1, mss << 1);
	}

	// Further, if the SYN or SYN/ACK is lost, the initial window used by a
	// sender after a correctly transmitted SYN MUST be one segment
	// consisting of at most SMSS bytes.

	// Furthermore, upon a timeout (as specified in [RFC2988]) cwnd MUST be
	// set to no more than the loss window, LW, which equals 1 full-sized
	// segment (regardless of the value of IW).  Therefore, after
	// retransmitting the dropped segment the TCP sender uses the slow start
	// algorithm to increase the window from 1 full-sized segment to the new
	// value of ssthresh, at which point congestion avoidance again takes
	// over.

	// Both aforementioned comments are implemented by setting CWND to MSS
	s->snd_cwnd = s->snd_mss;

	if (_verbose)
		click_chatter("%s: rtx, %s", class_name(), unparse(s).c_str());

	output(TCP_CCO_OUT_RTX_PORT).push(p); 
}

void
TCPCongestionControl::push(int port, Packet *p)
{
	switch (port) {
	case TCP_CCO_IN_SYN_PORT:
		handle_syn(p);
		break;
	case TCP_CCO_IN_ACK_PORT:
		handle_ack(p);
		break;
	case TCP_CCO_IN_OLD_PORT:
		handle_old(p);
		break;
	case TCP_CCO_IN_RTX_PORT:
		handle_rtx(p);
		break;
	}
}

String
TCPCongestionControl::unparse(TCPState *s) const
{
	StringAccum sa;
	sa << "cwnd " << s->snd_cwnd << ", ssthresh " << s->snd_ssthresh;
	return sa.take_string();
}

CLICK_ENDDECLS
#undef click_assert
EXPORT_ELEMENT(TCPCongestionControl)
