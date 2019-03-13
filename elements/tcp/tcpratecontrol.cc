/*
 * tcpratecontrol.{cc,hh} -- control transmission rate
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/ether.h>
#include "tcpratecontrol.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "util.hh"

CLICK_DECLS

TCPRateControl::TCPRateControl()
{
}

void
TCPRateControl::push(int, Packet *p)
{  
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	// If TX queue is empty or window is small, do not send any data. 
	if (s->txq.empty() || s->available_tx_window() < s->snd_mss) {
		if (TCP_ACK_FLAG_ANNO(p))  //Send empty packet if ACK REQUIRED flag set.
			output(0).push(p);
		else	
			p->kill(); //TODO Should we properly send Probe packet? Probing Zero Windows: RFC-1122 Section 4.2.2.17 and RFC-793 Section 3.7.
		return;
	}

	// Kill original packet, since it is gonna be replaced
	p->kill();

	// Get TX queue state
	bool txq_non_empty = !s->txq.empty();
//	bool txq_half_full = (s->txq.bytes() > (TCPInfo::wmem() >> 1));
	bool txq_half_full = (s->txq.bytes() < (TCPInfo::wmem()));

	// Keep sending until empty TX queue or small window
	while (!s->txq.empty() && s->available_tx_window() >= s->snd_mss) {
		// Get head-of-line (HOL) packet from TX queue
		Packet *q = s->txq.front();
		s->txq.pop_front();

		// Get length
		uint32_t len = q->length();

		// Send data packet
		SET_TCP_STATE_ANNO(q, (uint64_t)s);
		output(0).push(q);

		// Reacquire lock
//		s->lock.acquire();

		// Update state variable
		s->snd_nxt += len;
	}

	// Wake user task if waiting for an empty queue
	if (txq_non_empty && s->txq.empty())
		s->wake_up(TCP_WAIT_TXQ_EMPTY);

	// Wake up user task if waiting for space in the TX queue
//	if (txq_half_full && s->txq.bytes() <= (s->wmem >> 1))
	if (txq_half_full && s->txq.bytes() < (TCPInfo::wmem()))
		s->wake_up(TCP_WAIT_TXQ_HALF_EMPTY);

//	s->lock.release();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPRateControl)
