/*
 * bbrtcppacing.{cc,hh} -- schedules packets to be transmitted following pacing_rate
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
#include <click/error.hh>
#include <click/router.hh>
#include <click/tcpanno.hh>
#include "bbrtcppacing.hh"
#include "../tcpstate.hh"
CLICK_DECLS

BBRTCPPacing::BBRTCPPacing() {
}

Packet * BBRTCPPacing::smaction(Packet *p) {
	TCPState *s = TCP_STATE_ANNO(p);
	if (s->next_send_time == 0
			or (uint64_t)(s->next_send_time/1000)
					<= (uint64_t) Timestamp::now_steady().msecval()) {
        if (s->bbr->pacing_rate)
            s->next_send_time = (uint64_t)((uint64_t) Timestamp::now_steady().usecval()
				+ (uint32_t) (p->seg_len() * 1000000 / s->bbr->pacing_rate));
        else 
            s->next_send_time = (uint64_t) Timestamp::now_steady().usecval();
		return p;
	} else {
		s->bbr->pcq.push_back(p);
		if (!s->tx_timer.scheduled()) {
			s->tx_timer.schedule_after_msec(
					(uint64_t)((s->next_send_time
							- (uint64_t) Timestamp::now_steady().usecval())/1000));
            if (s->bbr->pacing_rate)
                s->next_send_time = (uint64_t)((uint64_t) Timestamp::now_steady().usecval()
					+ (uint32_t) (p->seg_len() * 1000000 / s->bbr->pacing_rate));
            else 
                s->next_send_time = (uint64_t) Timestamp::now_steady().usecval();
		}
	}
	return NULL;
}

void BBRTCPPacing::push(int, Packet *p) {
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
BBRTCPPacing::pull(int) {
	if (Packet *p = input(0).pull())
		smaction(p);
	return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BBRTCPPacing)
ELEMENT_MT_SAFE(BBRTCPPacing)
