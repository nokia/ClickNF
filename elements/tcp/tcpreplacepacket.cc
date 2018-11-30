/*
 * tcpreplacepacket.{cc,hh} -- Replace packet, preserving its timestamp
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
#include "tcpreplacepacket.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPReplacePacket::TCPReplacePacket()
{
}

Packet *
TCPReplacePacket::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);

	// Save timestamp
	Timestamp now = p->timestamp_anno();
	
	// Save ACK REQUIRED annotation
	bool ackreq = TCP_ACK_FLAG_ANNO(p);
	// If packet is shared, kill it and allocate a new one
	if (p->shared()) {
		// Kill packet
		p->kill();

		// Replace it with a new one
		WritablePacket *q = Packet::make(TCP_HEADROOM, NULL, 0, s->snd_mss);
		click_assert(q);

		// Load timestamp
		q->set_timestamp_anno(now);
		
		if(ackreq)
			SET_TCP_ACK_FLAG_ANNO(q);

		SET_TCP_STATE_ANNO(q, (uint64_t)s);
		return q;
	}

	// Otherwise, reuse the same packet
	p->reset();
	SET_TCP_STATE_ANNO(p, (uint64_t)s);

	// Load timestamp
	p->set_timestamp_anno(now);
	
	if(ackreq)
		SET_TCP_ACK_FLAG_ANNO(p);

	return p;
}

void
TCPReplacePacket::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPReplacePacket::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPReplacePacket)
