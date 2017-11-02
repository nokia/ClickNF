/*
 * tcprttestimator.{cc,hh} -- Estimate round-trip time (RTT)
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
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/tcp.h>
#include "tcpstate.hh"
#include "tcprttestimator.hh"
#include "util.hh"
CLICK_DECLS

TCPRttEstimator::TCPRttEstimator() : _verbose(false)
{
}

int
TCPRttEstimator::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	return 0;
}

Packet *
TCPRttEstimator::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	// Get RTT annotation
	uint32_t rtt = TCP_RTT_ANNO(p);

	// Header pointer
	const click_tcp *th = p->tcp_header();

	// TCP flags
	bool syn = (th->th_flags & TH_SYN);
	bool ack = (th->th_flags & TH_ACK);

	// If an incoming SYN packet, there is no RTT measurement
	if (syn && !ack)
		return p;

	// If there is no support for timestamps, take our own RTT measurements
	if (s->snd_ts_ok == false) {
		// Ignore old ACKs and ACKs for retransmissions (Karn's algorithm)
		if (!s->is_acceptable_ack(p) || s->snd_rtx_count > 0 || s->rtxq.empty())
			return p;

		// Otherwise, check if we have a valid RTT measurement
		Packet *x = s->rtxq.front();
		click_assert(x);
		const click_ip *xip = x->ip_header();
		const click_tcp *xth = x->tcp_header();
		uint32_t end = TCP_END(xip, xth);

		// Check if the ACK removes the HOL packet from the RTX queue
		if (SEQ_LT(end, TCP_ACK(th))) {
			Timestamp now = p->timestamp_anno();
			if (now == 0)
				now = Timestamp::now_steady();

			Timestamp rtt_ts = now - x->timestamp_anno();
			rtt = MAX(1, rtt_ts.usecval());
		}
	}
	// Otherwise, check for a valid RTT measurement
	else if (rtt == 0)
		return p;

	// RFC 6298:
	//
	// "The rules governing the computation of SRTT, RTTVAR, and RTO are as
	//  follows:
	//
	//  (2.1) Until a round-trip time (RTT) measurement has been made for a
	//        segment sent between the sender and receiver, the sender SHOULD
	//        set RTO <- 1 second, though the "backing off" on repeated
	//        retransmission discussed in (5.5) still applies.
	//
	//        Note that the previous version of this document used an initial
	//        RTO of 3 seconds [PA00].  A TCP implementation MAY still use
	//        this value (or any other value > 1 second).  This change in the
	//        lower bound on the initial RTO is discussed in further detail
	//        in Appendix A.
	//
	//  (2.2) When the first RTT measurement R is made, the host MUST set
	//
	//           SRTT <- R
	//           RTTVAR <- R/2
	//           RTO <- SRTT + max (G, K*RTTVAR)
	//
	//        where K = 4.
	//
	//  (2.3) When a subsequent RTT measurement R' is made, a host MUST set
	//
	//           RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R'|
	//           SRTT <- (1 - alpha) * SRTT + alpha * R'
	//
	//        The value of SRTT used in the update to RTTVAR is its value
	//        before updating SRTT itself using the second assignment.  That
	//        is, updating RTTVAR and SRTT MUST be computed in the above
	//        order.
	//
	//        The above SHOULD be computed using alpha=1/8 and beta=1/4 (as
	//        suggested in [JK88]).
	//
	//        After the computation, a host MUST update
	//        RTO <- SRTT + max (G, K*RTTVAR)
	//
	//  (2.4) Whenever RTO is computed, if it is less than 1 second, then the
	//        RTO SHOULD be rounded up to 1 second.
	//
	//        Traditionally, TCP implementations use coarse grain clocks to
	//        measure the RTT and trigger the RTO, which imposes a large
	//        minimum value on the RTO.  Research suggests that a large
	//        minimum RTO is needed to keep TCP conservative and avoid
	//        spurious retransmissions [AP99].  Therefore, this specification
	//        requires a large minimum RTO as a conservative approach, while
	//        at the same time acknowledging that at some future point,
	//        research may show that a smaller minimum RTO is acceptable or
	//        superior.
	//
	//  (2.5) A maximum value MAY be placed on RTO provided it is at least 60
	//        seconds.

	uint32_t rto;
	if (unlikely(s->snd_srtt == 0)) {
		s->snd_srtt = rtt;
		s->snd_rttvar = (rtt >> 1);
		rto = 3*rtt;
	}
	else {
		s->snd_rttvar = ((3*s->snd_rttvar + absdiff(s->snd_srtt, rtt)) >> 2);
		s->snd_srtt = ((7*s->snd_srtt + rtt) >> 3);
		rto = s->snd_srtt + MAX(1, (s->snd_rttvar << 2));
	}
	s->snd_rto = MIN(MAX(TCP_RTO_MIN, rto/1000), TCP_RTO_MAX);
	click_assert(s->snd_rto > 0);

	if (_verbose)
		click_chatter("%s: rtt %u us, srtt %u us, rttvar %u us, rto %u ms", \
		             class_name(), rtt, s->snd_srtt, s->snd_rttvar, s->snd_rto);
	return p;
}

void
TCPRttEstimator::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPRttEstimator::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPRttEstimator)
