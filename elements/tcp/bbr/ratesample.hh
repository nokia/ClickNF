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

#ifndef CLICK_RateSample_HH
#define CLICK_RateSample_HH
#include "../tcpstate.hh"
#include "pktstatequeue.hh"
#define BW_SCALE 24
#define BW_UNIT (1 << BW_SCALE)

CLICK_DECLS

class TCPState;


class RateSample { public:
	RateSample();
	~RateSample();
	void rate_check_app_limited(TCPState *s);


	uint64_t	prior_ustamp,
				interval_us,
				rtt_us;
	uint32_t	delivered,
				prior_delivered,
				snd_interval_us,
				rcv_interval_us,
				acked_sacked,
				prior_in_flight;

	bool		is_app_limited,
				is_retrans,
				is_ack_delayed;
	PktStateQueue pkt_states;


};
	

CLICK_ENDDECLS
#endif
