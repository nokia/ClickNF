/*
 * tcpcheckseqno.{cc,hh} -- Checks TCP sequence number
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
#include "tcpcheckseqno.hh"
#include "tcpstate.hh"
CLICK_DECLS

TCPCheckSeqNo::TCPCheckSeqNo()
{
}

Packet *
TCPCheckSeqNo::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && ip && th);

	// RFC 793:
	// "If an incoming segment is not acceptable, an acknowledgment
	//  should be sent in reply (unless the RST bit is set, if so drop
	//  the segment and return):
	//
	//     <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
	//
	//  After sending the acknowledgment, drop the unacceptable segment
	//  and return."
	uint32_t seq = TCP_SEQ(th);
	uint32_t sns = TCP_SNS(ip, th);

	if (unlikely(!s->is_acceptable_seq(seq, sns))) {
		if (TCP_RST(th)) {
			p->kill();
		}
		else if (TCP_SYN(th) && s->state == TCP_SYN_RECV) {
			s->rtx_timer.schedule_now();
			p->kill();
		}
		else
			output(1).push(p);

		return NULL;
	}

	return p;
}

void
TCPCheckSeqNo::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPCheckSeqNo::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPCheckSeqNo)
