/*
 * tcpsynoptionsparse.{cc,hh} -- Parse TCP SYN options
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
#include <clicknet/tcp.h>
#include <click/timestamp.hh>
#include "tcpsynoptionsparse.hh"
#include "tcpinfo.hh"
#include "tcpstate.hh"
#include "util.hh"
CLICK_DECLS

TCPSynOptionsParse::TCPSynOptionsParse()
{
}

Packet *
TCPSynOptionsParse::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	// Reset RTT annotation
	SET_TCP_RTT_ANNO(p, 0);

	const click_tcp *th = p->tcp_header();

	// If no options, return
	if (unlikely(th->th_off <= 5))
		return p;

	click_assert(TCP_SYN(th) && !TCP_RST(th) && !TCP_FIN(th));

	// Option headers
	const uint8_t *ptr = (const uint8_t *)(th + 1);
	const uint8_t *end = (const uint8_t *)th + (th->th_off << 2);

	// Process each option
	while (ptr < end) {
		uint8_t opcode = ptr[0];

		if (unlikely(opcode == TCPOPT_EOL))
			break;

		if (opcode == TCPOPT_NOP) {
			ptr++;
			continue;
		}

		// Stop if option is malformed
		if (unlikely(ptr + 1 == end || ptr + ptr[1] > end))
			break;

		uint8_t opsize = ptr[1];

		switch (opcode) {
		case TCPOPT_MAXSEG:           // MSS
			if (likely(opsize == TCPOLEN_MAXSEG)) {
				uint16_t mss = ntohs(*(const uint16_t *)&ptr[2]);
				s->snd_mss = MAX(TCP_SND_MSS_MIN, MIN(mss, TCP_SND_MSS_MAX));
			}
			break;

		case TCPOPT_WSCALE:           // Window scaling
			if (likely(opsize == TCPOLEN_WSCALE)) {
				// RFC 7323:
				// "Check for a Window Scale option (WSopt); if it is found,
				//  save SEG.WSopt in Snd.Wind.Shift; otherwise, set both
				//  Snd.Wind.Shift and Rcv.Wind.Shift to zero."
				s->snd_wscale_ok = true;
				s->snd_wscale = MIN(ptr[2], 14);
//				s->rcv_wscale = s->rcv_wscale_default;
				s->rcv_wscale = TCP_RCV_WSCALE_DEFAULT;
			}
			break;

		case TCPOPT_SACK_PERMITTED:   // SACK permitted
			if (likely(opsize == TCPOLEN_SACK_PERMITTED))
				s->snd_sack_permitted = true;
			break;

		case TCPOPT_TIMESTAMP:        // Timestamp
			if (likely(opsize == TCPOLEN_TIMESTAMP)) {
				// Get now, preferably from packet timestamp
				uint32_t now = (uint32_t)p->timestamp_anno().usecval();
				if (now == 0)
					now = (uint32_t)Timestamp::now_steady().usecval();

				// Get timestamp parameters
				uint32_t ts_val = ntohl(*(const uint32_t *)&ptr[2]);
				uint32_t ts_ecr = ntohl(*(const uint32_t *)&ptr[6]);

				// RFC 7323:
				// "Check for a TSopt option; if one is found, save SEG.TSval in
				//  variable TS.Recent and turn on the Snd.TS.OK bit in the
				//  connection control block.  If the ACK bit is set, use
				//  Snd.TSclock - SEG.TSecr as the initial RTT estimate."
				s->snd_ts_ok = true;
				s->ts_recent = ts_val;
				s->ts_recent_update = now;

				// Update RTT estimate if this is a SYN-ACK
				if (th->th_flags & TH_ACK) {
					ts_ecr -= s->ts_offset;
					SET_TCP_RTT_ANNO(p, MAX(1, now - ts_ecr));

					// FIXME: fails if first SYN is retransmitted
					SET_TCP_RTT_ANNO(p, 0);
				}
			}
			break;

		default:
			break;
		}

		ptr += opsize;
	}

	return p;
}

void
TCPSynOptionsParse::push(int, Packet *p)
{
	p = smaction(p);
	if (likely(p))
		output(0).push(p);
}

Packet *
TCPSynOptionsParse::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPSynOptionsParse)
