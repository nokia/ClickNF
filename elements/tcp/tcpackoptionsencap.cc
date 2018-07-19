/*
 * tcpoptionsencap.{cc,hh} -- encapsulates packet with TCP options
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpackoptionsencap.hh"
#include "tcpsack.hh"
#include "tcpstate.hh"
#include "util.hh"
CLICK_DECLS

TCPAckOptionsEncap::TCPAckOptionsEncap()
{
}

uint8_t
TCPAckOptionsEncap::oplen(TCPState *s)
{
	click_assert(s);

	uint8_t oplen = 0;

	if (s->snd_ts_ok)
		oplen += 12;

	if (s->snd_sack_permitted && !s->rxb.empty()) {
		TCPSack sack = s->rxb.sack();
		uint8_t max_blocks = (s->snd_ts_ok ? 3 : 4);
		uint8_t blocks = MIN(max_blocks, sack.blocks());

		oplen += (4 + 8*blocks);
	}

	return oplen;
}

uint8_t
TCPAckOptionsEncap::min_oplen(TCPState *s)
{
	click_assert(s);

	uint8_t oplen = 0;

	if (s->snd_ts_ok)
		oplen += 12;

	return oplen;
}

uint8_t
TCPAckOptionsEncap::max_oplen(TCPState *s)
{
	click_assert(s);

	uint8_t oplen = 0;

	if (s->snd_ts_ok)
		oplen += 12;

	if (s->snd_sack_permitted) {
		uint8_t max_blocks = (s->snd_ts_ok ? 3 : 4);
		oplen += (4 + 8*max_blocks);
	}

	return oplen;
}

Packet *
TCPAckOptionsEncap::smaction(Packet *q)
{
	TCPState *s = TCP_STATE_ANNO(q);
	click_assert(s);

	WritablePacket *p = q->uniqueify();
	click_assert(p);

	// Options length annotation
	uint8_t oplen = 0;

	// TCP Timestamp
	if (s->snd_ts_ok) {
		// Add space for timestamp option
		p = p->push(12);
		click_assert(p);

		// Update options length annotation
		oplen += 12;

		// Pointer to the new space
		uint8_t *ptr = p->data();

		// TCP timestamp
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_TIMESTAMP;
		ptr[3] = TCPOLEN_TIMESTAMP;

		// Get now, preferably from packet timestamp
		uint32_t now = (uint32_t)p->timestamp_anno().usecval();
		if (now == 0)
			now = (uint32_t)Timestamp::now_steady().usecval();

		// Pointers to the timestamps
		uint32_t *ts_val = (uint32_t *)(ptr + 4);
		uint32_t *ts_ecr = (uint32_t *)(ptr + 8);

		*ts_val = htonl(s->ts_offset + now);
		*ts_ecr = htonl(s->ts_recent);

		if (SEQ_GT(s->rcv_nxt, s->ts_last_ack_sent))
			s->ts_last_ack_sent = s->rcv_nxt;
	}

	// Selective ACK (SACK)
	if (s->snd_sack_permitted && !s->rxb.empty()) {
		TCPSack sack = s->rxb.sack();
		uint8_t max_blocks = (s->snd_ts_ok ? 3 : 4);
		uint8_t blocks = MIN(max_blocks, sack.blocks());

		// Add space for SACK option
		p = p->push(4 + 8*blocks);
		click_assert(p);

		// Update options length annotation
		oplen += (4 + 8*blocks);

		// Pointer to the new space
		uint8_t *ptr = p->data();

		// TCP SACK
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_SACK;
		ptr[3] = 2 + 8*blocks;
		ptr += 4;

		for (int i = 0; i < blocks; i++) {
			uint32_t *l = (uint32_t *)(ptr + 0);
			uint32_t *r = (uint32_t *)(ptr + 4);

			*l = htonl(sack[i].left());
			*r = htonl(sack[i].right());

			ptr += 8;
		}
	}

	SET_TCP_OPLEN_ANNO(p, oplen);
	return p;
}

void
TCPAckOptionsEncap::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPAckOptionsEncap::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPAckOptionsEncap)
