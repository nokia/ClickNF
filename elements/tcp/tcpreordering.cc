/*
 * tcpreordering.{cc,hh} -- Guarantees in-order delivery of TCP segments
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
#include "tcpreordering.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "util.hh"
CLICK_DECLS

TCPReordering::TCPReordering()
{
}

void
TCPReordering::push(int, Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
//	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);

	// RFC 793:
	// "In the following it is assumed that the segment is the idealized
	//  segment that begins at RCV.NXT and does not exceed the window.
	//  One could tailor actual segments to fit this assumption by
	//  trimming off any portions that lie outside the window (including
	//  SYN and FIN), and only processing further if the segment then
	//  begins at RCV.NXT.  Segments with higher begining sequence
	//  numbers may be held for later processing."
	if (likely(TCP_SEQ(th) == s->rcv_nxt && s->rxb.empty())) {
		RESET_TCP_MS_FLAG_ANNO(p);
		RESET_TCP_ACK_FLAG_ANNO(p);
		output(0).push(p);
		return;
	}

	// Reset state annotation as the lock is not held while in the buffer
	SET_TCP_STATE_ANNO(p, 0);

	// Insert packet into RX buffer and get amount of added data
	int data = s->rxb.insert(p);

	// If packet is not added to the RX buffer, kill it as it is a duplicate
	if (data < 0) {
		p->kill();
		p = NULL;
	}
	else {
		// Packet is not accessible anymore since it is in the RX buffer
		p = NULL;

		// Reduce window with new data in RX buffer (it does not include FIN)
		s->rcv_wnd -= data;

		// If gap is filled, remove a packet from RX buffer and process it
		while ((p = s->rxb.remove(s->rcv_nxt))) {
			uint16_t len = TCP_LEN(p);

			// Inflate receive window (deflated when segment text is processed)
			s->rcv_wnd += len;

			// If more segments are coming, set flag to avoid premature ACKs
			if (s->rxb.peek(s->rcv_nxt + len)) {
				SET_TCP_MS_FLAG_ANNO(p);
				RESET_TCP_ACK_FLAG_ANNO(p);
			}
			else {
				RESET_TCP_MS_FLAG_ANNO(p);
				SET_TCP_ACK_FLAG_ANNO(p);
			}

			// Set packet annotation
			SET_TCP_STATE_ANNO(p, (uint64_t)s);

			// Process packet
			output(0).push(p);

		}
	}

	// If nothing was removed from the RX buffer, send an ACK
	if (!p) {
#if HAVE_TCP_DELAYED_ACK
		// Stop delayed ACK timer
		s->delayed_ack_timer.unschedule();
#endif

		// Create packet for the ACK
		WritablePacket *q = Packet::make(TCP_HEADROOM, NULL, 0, s->snd_mss);
		click_assert(q);
			
		// Set packet annotation
		SET_TCP_STATE_ANNO(q, (uint64_t)s);

		// Send ACK
		output(1).push(q);
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPReordering)

