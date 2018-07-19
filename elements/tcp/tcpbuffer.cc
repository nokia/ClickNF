/*
 * tcpbuffer.{cc,hh} -- buffer TCP packets in sequence-number order
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
#include <click/packet.hh>
#include <click/string.hh>
#include <click/straccum.hh>
#include <clicknet/tcp.h>
#include "tcpbuffer.hh"
#include "tcptrimpacket.hh"
#include "tcpsack.hh"
CLICK_DECLS

TCPBuffer::TCPBuffer() : PktQueue(), _last(NULL)
{
}

TCPBuffer::~TCPBuffer()
{
	flush();
}

int
TCPBuffer::insert(Packet *p)
{
	// Header pointers
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();

	// First and last sequence numbers
	uint32_t seq = TCP_SEQ(th);
	uint32_t end = TCP_END(ip, th);

	// If buffer is empty, just insert the packet and return
	if (empty()) {
		_last = p;
		push_back(p);
		return TCP_LEN(ip, th);
	}

	// Keep track of the buffered sequence number space
	int length = 0;
	bool inserted = false;

	// Starting from the tail, insert packet in list ordered by sequence number
	Packet *x = back();
	do {
		const click_ip *xip = x->ip_header();
		const click_tcp *xth = x->tcp_header();

		uint32_t xseq = TCP_SEQ(xth);
		uint32_t xend = TCP_END(xip, xth);

		// Comparisons with each packet in the buffer to prevent data overlap
		//
		//                          xseq         xend
		//                            |           |
		//                            v           v
		//                            +===========+
		//                            |     x     |         seq       end
		//                            +===========+          |         |
		//                            |           |          v         v
		//                            |           |          +=========+
		//  (1)                       |           |          |    p    |
		//                            |           |          +=========+
		//                            |        +==+======+
		//  (2.1)                     |        |  | p    |
		//                            |        +==+======+
		//                            |           |
		//                          +=============+=====+
		//  (2.2)                   | |       p   |     |
		//                          +=============+=====+
		//                            |           |
		//                            | +=========+
		//  (3)                       | |    p    |
		//                            | +=========+
		//                            |           |
		//                     +======+==+        |
		//  (4)                |    p |  |        |
		//                     +======+==+        |
		//                            |           |
		//       +=========+          |           |
		//  (5)  |    p    |          |           |
		//       +=========+          |           |
		//                            |           |
		//

		// (1) Packet sequence space does not overlap
		if (SEQ_LT(xend, seq)) {
			_last = p;
			insert_after(x, p);
			length += TCP_LEN(ip, th);
			return length;
		}

		// (2) Beginning of packet overlaps
		if (SEQ_LT(xend, end)) {

			// (2.2) If seq < xseq, make a copy of the packet to check head data
			Packet *q = NULL;
			if (SEQ_LT(seq, xseq)) {
				// Clone the packet
				q = p->clone();
				click_assert(q);

				// Trim end of the packet
				q = TCPTrimPacket::trim_end(q, end - xseq + 1);
				click_assert(q);
			}

			// Amount of redundant data to trim in the beginning
			uint32_t delta = xend - seq + 1;

			// Trim and reset pointers
			p = TCPTrimPacket::trim_begin(p, delta);
			click_assert(p);
			ip = p->ip_header();
			th = p->tcp_header();

			// Make sure there is no overlap in the sequence space
			click_assert(TCP_SEQ(th) == xend + 1);

			// Insert packet in the buffer
			_last = p;
			insert_after(x, p);
			length += TCP_LEN(ip, th);
			inserted = true;

			// If seq >= xseq, we are done
			if (q == NULL)
				return length;
			else {
				p = q;
				ip = p->ip_header();
				th = p->tcp_header();
				// Continue processing
			}
		}

		// (3) Entire packet overlaps
		if (SEQ_LEQ(xseq, seq))
			return inserted ? length : -EEXIST;

		// (4) End of packet overlaps
		if (SEQ_LEQ(xseq, end)) {
			// Amount of redundant data to trim in the end
			uint32_t delta = end - xseq + 1;

			// Trim and reset pointers
			p = TCPTrimPacket::trim_end(p, delta);
			click_assert(p);
			ip = p->ip_header();
			th = p->tcp_header();

			// Make sure there is no overlap in the sequence space
			click_assert(TCP_END(ip, th) + 1 == xseq);

			// Do not insert it yet, still need to check against previous packet
		}

		// Get previous packet
		x = x->prev();

	} while (x != back());

	// Packet sequence space lower than all packets in the buffer
	click_assert(SEQ_LT(end, TCP_SEQ(front()->tcp_header())));
	_last = p;
	push_front(p);
	length += TCP_LEN(ip, th);

	return length;
}

TCPSack
TCPBuffer::sack() const
{
	TCPSack sack;

	// Return an empty vector if buffer is empty
	if (empty())
		return sack;

	// Get first packet
	Packet *p = front();

	uint32_t seq = TCP_SEQ(p->tcp_header());
	uint32_t end = seq - 1;

	// Get continuous sequence spaces
	do {
		const click_ip *ip = p->ip_header();
		const click_tcp *th = p->tcp_header();

		uint32_t xseq = TCP_SEQ(th);
		uint32_t xend = TCP_END(ip, th);

		if (end + 1 != xseq) {
			sack.insert_block(TCPSackBlock(seq, end + 1));
			seq = xseq;
		}

		// Update last seen sequence numbers
		end = xend;

		// Get next packet in the buffer
		p = p->next();

	} while (p != front());

	// Push back the last block
	sack.insert_block(TCPSackBlock(seq, end + 1));

	// RFC 2018:
	//  * The first SACK block (i.e., the one immediately following the
	//    kind and length fields in the option) MUST specify the contiguous
	//    block of data containing the segment which triggered this ACK,
	//    unless that segment advanced the Acknowledgment Number field in
	//    the header.  This assures that the ACK with the SACK option
	//    reflects the most recent change in the data receiver's buffer
	//    queue.
	if (_last) {
		seq = TCP_SEQ(_last);
		end = TCP_END(_last);

		for (size_t i = 0; i < sack.blocks(); i++) {
			uint32_t l = sack[i].left();
			uint32_t r = sack[i].right();

			//If this block contains the last inserted segment, send it first
			if (SEQ_LEQ(l, seq) && SEQ_LT(end, r)) {
				if (i != 0) {
					TCPSackBlock b = sack[0];
					sack[0] = sack[i];
					sack[i] = b;
				}
				break;
			}
		}
	}

	return sack;
}

bool
TCPBuffer::peek(uint32_t rcv_nxt)
{
	// Return if buffer is empty
	if (empty())
		return false;

	// Get first packet
	Packet *p = front();

	// Get sequence number of first packet
	uint32_t seq = TCP_SEQ(p);

	// Make sure packets are ordered
	click_assert(SEQ_LEQ(rcv_nxt, seq));

	// If same sequence number, return true
	if (seq == rcv_nxt)
		return true;

	return false;
}

Packet *
TCPBuffer::remove(uint32_t rcv_nxt)
{
	// Return if buffer is empty
	if (empty())
		return NULL;

	// Get first packet
	Packet *p = front();

	// Get sequence number of first packet
	uint32_t seq = TCP_SEQ(p);

	// Make sure packets are ordered
	click_assert(SEQ_LEQ(rcv_nxt, seq));

	// If same sequence number, remove the packet from the buffer
	if (seq == rcv_nxt) {
		_last = NULL;
		pop_front();
		return p;
	}

	return NULL;
}

String
TCPBuffer::unparse() const
{
	StringAccum sa;
	sa << "TCPBuffer\n";

	if (empty()) {
		sa << "  Empty\n";
		return sa.take_string();
	}
		
	// Get first packet
	Packet *p = front();

	// Go over each packet and get its sequence space
	do {
		const click_ip *ip = p->ip_header();
		const click_tcp *th = p->tcp_header();

		uint32_t seq = TCP_SEQ(th);
		uint32_t end = TCP_END(ip, th);

		// Print first (inclusive) and last (exclusive) sequence numbers
		sa << "  " << seq << ":" << end + 1 << "\n";

		// Get next packet in the buffer
		p = p->next();

	} while (p != front());

	return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PktQueue)
ELEMENT_PROVIDES(TCPBuffer)
