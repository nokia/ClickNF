/*
 * tcpupdatetimestamp.{cc,hh} -- Updates TCP timestamp
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
#include "tcpupdatetimestamp.hh"
#include "tcpstate.hh"
#include "util.hh"
CLICK_DECLS

TCPUpdateTimestamp::TCPUpdateTimestamp()
{
}

Packet *
TCPUpdateTimestamp::smaction(Packet *q)
{
	TCPState *s = TCP_STATE_ANNO(q);
	click_assert(s);

	WritablePacket *p = q->uniqueify();
	assert(p);

	click_tcp *th = p->tcp_header();
	uint8_t *ptr = (uint8_t *)(th + 1);
	uint8_t *end = (uint8_t *)th + (th->th_off << 2);

	// Update timestamp
	while (ptr < end) {
		uint8_t opcode = ptr[0];

		if (opcode == TCPOPT_EOL)
			break;

		if (opcode == TCPOPT_NOP) {
			ptr++;
			continue;
		}

		// Make sure that option is not malformed
		click_assert(ptr + 1 < end && ptr + ptr[1] <= end);

		uint8_t opsize = ptr[1];

		if (opcode == TCPOPT_TIMESTAMP && opsize == TCPOLEN_TIMESTAMP) {
			uint32_t *ts_val = (uint32_t *)(ptr + 2);
			uint32_t *ts_ecr = (uint32_t *)(ptr + 6);

			uint32_t now = (uint32_t)Timestamp::now_steady().usecval();

			*ts_val = htonl(s->ts_offset + now);
			*ts_ecr = htonl(s->ts_recent);
		}

		ptr += opsize;
	}

	return p;
}

void
TCPUpdateTimestamp::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPUpdateTimestamp::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPUpdateTimestamp)
