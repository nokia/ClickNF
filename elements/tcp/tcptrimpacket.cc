/*
 * tcptrimpacket.{cc,hh} -- Trims off TCP packet to fit the receive window
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
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcptrimpacket.hh"
#include "tcpstate.hh"
#include "util.hh"
CLICK_DECLS

TCPTrimPacket::TCPTrimPacket()
{
}

Packet *
TCPTrimPacket::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && ip && th);

	// Get first and last sequence numbers
	uint32_t seq = TCP_SEQ(th);
	uint32_t end = TCP_END(ip, th);
	
	// RFC 793:
	// "In the following it is assumed that the segment is the idealized
	//  segment that begins at RCV.NXT and does not exceed the window.
	//  One could tailor actual segments to fit this assumption by
	//  trimming off any portions that lie outside the window (including
	//  SYN and FIN), and only processing further if the segment then
	//  begins at RCV.NXT.  Segments with higher begining sequence
	//  numbers may be held for later processing."

	// Trim off beginning of packet to fit our window
	if (unlikely(SEQ_LT(seq, s->rcv_nxt))) {
		// Amount to trim
		uint16_t delta = s->rcv_nxt - seq;

		// Trim off packet
		p = trim_begin(p, delta);
	}

	// Trim off end of packet to fit our window
	if (unlikely(SEQ_GT(end, s->rcv_nxt + s->rcv_wnd - 1))) {
		// Amount to trim
		uint16_t delta = end - (s->rcv_nxt + s->rcv_wnd - 1);

		// Trim off packet
		p = trim_end(p, delta);
	}

	return p;
}

WritablePacket *
TCPTrimPacket::trim_begin(Packet *p, uint16_t delta)
{
	click_assert(delta <= TCP_SNS(p));

	// Prepare packet for writing
	WritablePacket *wp = p->uniqueify();
	if (!wp)
		return NULL;

	// If nothing to trim, just return
	if (delta == 0)
		return wp;

	// Header pointers
	click_ip *ip = wp->ip_header();
	click_tcp *th = wp->tcp_header();

	// Get data length
	uint32_t len = TCP_LEN(ip, th);

	// Adjust TCP sequence number
	th->th_seq = htonl(TCP_SEQ(th) + delta);

	// Reset the SYN flag, since it holds the first sequence number
	if (TCP_SYN(th)) {
		th->th_flags &= ~TH_SYN;
		delta--;
	}

	// Reset the FIN flag before adjusting data
	if (TCP_FIN(th) && delta == len + 1) {
		th->th_flags &= ~TH_FIN;
		delta--;
	}

	// Delta now represents the amount of data to trim off
	if (delta > 0) {
		// Adjust IP length
		ip->ip_len = htons(ntohs(ip->ip_len) - delta);

		// Source and destination initial segments
		WritablePacket *spkt = wp;
		WritablePacket *dpkt = wp;

		// Source and destination initial data offsets
		uint16_t doff = (ip->ip_hl + th->th_off) << 2;
		uint16_t soff = doff + delta;

		// Find source segment and offset
		while (soff > 0) {
			if (soff < spkt->seg_len())
				break;

			soff -= spkt->seg_len();
			spkt = spkt->seg_next();
		}

		// Amount of data to be copied
		len -= delta;

		// Copy data from source to destination segment one chunk at a time
		while (len > 0) {
			// Data pointers
			uint8_t *src = spkt->data() + soff;
			uint8_t *dst = dpkt->data() + doff;

			// Byte count
			uint16_t cnt = MIN(spkt->seg_len() - soff, dpkt->seg_len() - doff);

			// Data copy
			memmove(dst, src, cnt);

			// Offset update
			soff += cnt;
			doff += cnt;

			// Get next segment
			if (soff == spkt->seg_len()) {
				soff = 0;
				spkt = spkt->seg_next();
			}
			if (doff == dpkt->seg_len()) {
				doff = 0;
				dpkt = dpkt->seg_next();
			}

			// Reduce remaining length
			len -= cnt;
		}

		wp->seg_take(delta);

	}

	return wp;
}


WritablePacket *
TCPTrimPacket::trim_end(Packet *p, uint16_t delta)
{
	click_assert(delta <= TCP_SNS(p));

	// Prepare packet for writing
	WritablePacket *wp = p->uniqueify();
	click_assert(wp);

	// If nothing to trim, just return
	if (delta == 0)
		return wp;

	// Header pointers
	click_ip *ip = wp->ip_header();
	click_tcp *th = wp->tcp_header();

	// Get data length
	uint32_t len = TCP_LEN(ip, th);

	// Reset the FIN flag, since it holds the last sequence number
	if (TCP_FIN(th)) {
		th->th_flags &= ~TH_FIN;
		delta--;
	}

	// Reset the SYN flag before adjusting data
	if (TCP_SYN(th) && delta == len + 1) {
		th->th_flags &= ~TH_SYN;
		delta--;
	}

	// Delta now represents the amount of data to trim off
	if (delta > 0) {
		// Adjust IP length
		ip->ip_len = htons(ntohs(ip->ip_len) - delta);

		// Trim off end of the packet
		wp->seg_take(delta);
	}

	return wp;
}

void
TCPTrimPacket::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPTrimPacket::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPTrimPacket)
