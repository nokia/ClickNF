/*
 * tcpsegmentation.{cc,hh} -- Segment TCP packet into MTU-sized packets
 * Rafael Laufer
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
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpsegmentation.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "util.hh"
CLICK_DECLS

TCPSegmentation::TCPSegmentation()
{
}

void
TCPSegmentation::push(int, Packet *p)
{
	// No support for multisegment packets for now
	click_assert(TCP_MSS_ANNO(p) && p->segments() == 1);

	static int chatter = 0;

	// Header pointer
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(ip && th);

	uint8_t hlen = (ip->ip_hl + th->th_off) << 2;  // Header length
	uint32_t len = p->length() - hlen;             // Data length
	uint32_t seq = TCP_SEQ(th);                    // Sequence number

	// MSS annotation is already reduced by TCP option size (RFC 6691)
	uint32_t mss = TCP_MSS_ANNO(p);

	// If data length is less than or equal to MSS, no need for segmentation
	if (len <= mss) {
		output(0).push(p);
		return;
	}

	// Notify that segmentation is happening
	if (chatter < 5) {
		click_chatter("%s: len %u, mss %u", class_name(), len, mss);
		chatter++;
	}

	// TCP segmentation
	for (uint32_t offset = 0; offset < len; offset += mss) {
		bool first = (offset == 0);
		bool last = (offset + mss >= len);
		WritablePacket *q;

		if (!last) {
			// Create packet
			q = Packet::make(TCP_HEADROOM, NULL, 0, mss);
			click_assert(q);

			// Copy header
			q = q->push(hlen);
			click_assert(q);
			memcpy(q->data(), p->data(), hlen);

			// Copy data
			q = q->put(mss);
			click_assert(q);
			memcpy(q->data() + hlen, p->data() + hlen + offset, mss);
		}
		else {
			// Reuse original packet
			q = p->uniqueify();
			click_assert(q);

			// Copy header
			memcpy(q->data() + offset, q->data(), hlen);
			q->pull(offset);
		}

		// Set IP header
		q->set_ip_header((click_ip *)q->data(), ip->ip_hl << 2);

		// Get TCP header pointer
		click_tcp *th = q->tcp_header();

		// If SYN is on, turn it off except if this is the first packet
		if (TCP_SYN(th) && !first) {
			th->th_flags &= ~TH_SYN;
			seq++;
		}

		// If FIN is on, turn it off except if this is the last packet
		if (TCP_FIN(th) && !last)
			th->th_flags &= ~TH_FIN;

		// Fix sequence number
		th->th_seq = htonl(seq + offset);

		// Send segment
		output(0).push(q);
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPSegmentation)

