/*
 * ratesample.{cc,hh} -- Delivery Rate Estimation  draft-cardwell-iccrg-bbr-congestion-control-00
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
#include <click/packet.hh>
#include "ratesample.hh"


RateSample::RateSample()
:	prior_ustamp(0),
	interval_us(0),
	rtt_us(0),
	delivered(0),
	prior_delivered(0),
	snd_interval_us(0),
	rcv_interval_us(0),
	acked_sacked(0),
	prior_in_flight(0),
	is_app_limited(0),
	is_retrans(0),
	is_ack_delayed(0)
{
}

RateSample::~RateSample() {
}

void RateSample::rate_check_app_limited(TCPState *s){

		if (/* We have less than one packet to send. */
		    s->snd_una - s->snd_nxt < s->snd_mss &&
		    /* Nothing in sending host's qdisc queues or NIC tx queue. */
			s->txq.packets() < 1 &&
		    /* We are not limited by CWND. */
			s->tcp_packets_in_flight() < s->snd_cwnd &&
		    /* All lost packets have been retransmitted. */
		    s->rtxq.packets() == 0)
			s->app_limited =
				(s->delivered + s->tcp_packets_in_flight()) ? : 1;
}
CLICK_ENDDECLS
#undef click_assert
ELEMENT_PROVIDES(RateSample)
