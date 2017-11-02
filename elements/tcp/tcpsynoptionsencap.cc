/*
 * tcpsynoptionsencap.{cc,hh} -- encapsulates packet with TCP SYN options
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
#include "tcpsynoptionsencap.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPSynOptionsEncap::TCPSynOptionsEncap()
{
}

Packet *
TCPSynOptionsEncap::smaction(Packet *q)
{
	TCPState *s = TCP_STATE_ANNO(q);
	click_assert(s);

	// Prepare packet for writing
	WritablePacket *p = q->uniqueify();
	click_assert(p);

	// Options size
	uint8_t oplen = 0;

	// Option data pointer
	uint8_t *ptr;

	// Only add SACK-PERMITTED in the SYN-ACK if we saw it in the SYN first
	if (!s->is_passive || s->snd_sack_permitted) {
		// Add space for options
		p = p->push(2);
		click_assert(p);
		ptr = p->data();
		oplen += 2;

		// SACK permitted
		ptr[0] = TCPOPT_SACK_PERMITTED;
		ptr[1] = TCPOLEN_SACK_PERMITTED;
	}

	// Only add timestamp in the SYN-ACK if we saw it in the SYN first
	if (!s->is_passive || s->snd_ts_ok) {
		// Add two padding bytes, if needed
		if (oplen == 0) {
			p = p->push(2);
			click_assert(p);
			ptr = p->data();
			oplen += 2;

			ptr[0] = TCPOPT_NOP;
			ptr[1] = TCPOPT_NOP;
		}

		// Add space for options
		p = p->push(10);
		click_assert(p);
		ptr = p->data();
		oplen += 10;

		// Sample random offset
		if (s->ts_offset == 0)
			s->ts_offset = click_random(1, 0xFFFFFFFFU);

		// Get now, preferably from packet timestamp
		uint32_t now = (uint32_t)p->timestamp_anno().usecval();
		if (now == 0)
			now = (uint32_t)Timestamp::now_steady().usecval();

		// TCP timestamp
		ptr[0] = TCPOPT_TIMESTAMP;
		ptr[1] = TCPOLEN_TIMESTAMP;

		// Pointers to the timestamps
		uint32_t *ts_val = (uint32_t *)(ptr + 2);
		uint32_t *ts_ecr = (uint32_t *)(ptr + 6);

		*ts_val = htonl(s->ts_offset + now);
		*ts_ecr = htonl(s->is_passive ? s->ts_recent : 0);

		if (s->is_passive)
			s->ts_last_ack_sent = s->rcv_nxt;
	}

	// Only add window scale in the SYN-ACK if we saw it in the SYN first
	if (!s->is_passive || s->snd_wscale_ok) {
		// Add space for options
		p = p->push(4);
		click_assert(p);
		ptr = p->data();
		oplen += 4;

		// Window scale
		ptr[0] = TCPOPT_WSCALE;
		ptr[1] = TCPOLEN_WSCALE;
//		ptr[2] = s->rcv_wscale_default;
		ptr[2] = TCP_RCV_WSCALE_DEFAULT;
		ptr[3] = TCPOPT_NOP;
	}

	// RFC 6691:
	// "The MSS value to be sent in an MSS option should be equal to the
	//  effective MTU minus the fixed IP and TCP headers.  By ignoring both
	//  IP and TCP options when calculating the value for the MSS option, if
	//  there are any IP or TCP options to be sent in a packet, then the
	//  sender must decrease the size of the TCP data accordingly."

	// Add space for options
	p = p->push(4);
	click_assert(p);
	ptr = p->data();
	oplen += 4;

	// Maximum segment size
	ptr[0] = TCPOPT_MAXSEG;
	ptr[1] = TCPOLEN_MAXSEG;
	ptr[2] = s->rcv_mss >> 8;
	ptr[3] = s->rcv_mss & 0xff;

	SET_TCP_OPLEN_ANNO(p, oplen);
	return p;
}

void
TCPSynOptionsEncap::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPSynOptionsEncap::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPSynOptionsEncap)
