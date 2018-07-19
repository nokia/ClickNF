/*
 * tcpackoptionsparse.{cc,hh} -- Parse TCP ACK options
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
#include "tcpackoptionsparse.hh"
#include "tcpinfo.hh"
#include "tcpstate.hh"
#include "util.hh"
CLICK_DECLS

TCPAckOptionsParse::TCPAckOptionsParse()
{
}

Packet *
TCPAckOptionsParse::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);

	// Reset RTT annotation
	SET_TCP_RTT_ANNO(p, 0);

	// If no options or a SYN retransmission, return
	if (th->th_off <= 5 || TCP_SYN(th))
		return p;

	// Option headers
	const uint8_t *ptr = (const uint8_t *)(th + 1);
	const uint8_t *end = (const uint8_t *)th + (th->th_off << 2);

	// Process each option
	while (ptr < end) {
		uint8_t opcode = ptr[0];

		if (unlikely(opcode == TCPOPT_EOL))
			break;

		if (unlikely(opcode == TCPOPT_NOP)) {
			ptr++;
			continue;
		}

		// Stop if option is malformed
		if (unlikely(ptr + 1 == end || ptr + ptr[1] > end))
			break;

		uint8_t opsize = ptr[1];

		switch (opcode) {
		case TCPOPT_SACK:
			// Process SACK
			if (s->snd_sack_permitted && opsize >= 10 && opsize <= 34) {

				// Make sure the RTX queue is not empty and size is correct
				if (s->rtxq.empty() || ((opsize - 2) % 8) != 0)
					break;

				// Get the number of blocks
				uint8_t blocks = (opsize - 2) >> 3;

				// Get head of the RTX queue
				Packet *q = s->rtxq.front();

				// For each packet in the RTX queue, check it against the blocks
				do {
					uint32_t qseq = TCP_SEQ(q);
					uint32_t qend = TCP_END(q);

					for (uint8_t i = 0; i < blocks; i++) {
						// First and last sequence numbers of block
						uint32_t bseq = ntohl(*(const uint32_t *)&ptr[2+8*i]);
						uint32_t bend = ntohl(*(const uint32_t *)&ptr[6+8*i])-1;

						if (SEQ_LEQ(bseq, qseq) && SEQ_LEQ(qend, bend)) {
							SET_TCP_SACK_FLAG_ANNO(q);
							break;
						}
					}

					q = q->next();

				} while (q != s->rtxq.front());
			}
			break;

		case TCPOPT_TIMESTAMP:
			if (opsize == TCPOLEN_TIMESTAMP && s->snd_ts_ok) {
				// Get now, preferably from packet timestamp
				uint32_t now = (uint32_t)p->timestamp_anno().usecval();
				if (now == 0)
					now = (uint32_t)Timestamp::now_steady().usecval();

				// Get timestamp parameters
				uint32_t ts_val = ntohl(*(const uint32_t *)&ptr[2]);
				uint32_t ts_ecr = ntohl(*(const uint32_t *)&ptr[6]);

				// RFC 7323:
				// "If SEG.TSval < TS.Recent and the RST bit is off:
				//    If the connection has been idle more than 24 days,
				//    save SEG.TSval in variable TS.Recent, else the segment
				//    is not acceptable; follow the steps below for an
				//    unacceptable segment.
				//
				//  If an incoming segment is not acceptable, an 
				//  acknowledgment should be sent in reply (unless the RST 
				//  bit is set; if so drop the segment and return):
				//
				//          <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
				//
				//  Last.ACK.sent is set to SEG.ACK of the acknowledgment.  
				//  If the Snd.TS.OK bit is on, include the Timestamps 
				//  option <TSval=Snd.TSclock,TSecr=TS.Recent> in this <ACK>
				//  segment. Set Last.ACK.sent to SEG.ACK and send the <ACK>
				//  segment. After sending the acknowledgment, drop the 
				//  unacceptable segment and return."
				if (SEQ_LT(ts_val, s->ts_recent) && !TCP_RST(th)) {
					uint32_t twenty_four_days_ms = 24 * 24 * 60 * 60 * 1000;
					if (SEQ_GT(now, s->ts_recent_update + twenty_four_days_ms)){
						s->ts_recent = ts_val;
						s->ts_recent_update = now;
					}
					else {
						s->ts_last_ack_sent = s->rcv_nxt;
						output(1).push(p);  // ACK
						return NULL;
					}
				}

				// RFC 7323:
				// "If SEG.TSval >= TS.Recent and SEG.SEQ <= Last.ACK.sent,
				//  then save SEG.TSval in variable TS.Recent."
				if (SEQ_GEQ(ts_val, s->ts_recent) && \
				      SEQ_LEQ(TCP_SEQ(th), s->ts_last_ack_sent)) {
					s->ts_recent = ts_val;
					s->ts_recent_update = now;
				}

				// "ESTABLISHED STATE
				//
				//     If SND.UNA < SEG.ACK <= SND.NXT then, set SND.UNA <-
				//     SEG.ACK.  Also compute a new estimate of round-trip time.
				//     If Snd.TS.OK bit is on, use Snd.TSclock - SEG.TSecr;
				//     otherwise, use the elapsed time since the first segment
				//     in the retransmission queue was sent."
				//
				// Also allow SYN_RECV and CLOSE_WAIT state
				if ((s->state == TCP_ESTABLISHED || \
				     s->state == TCP_CLOSE_WAIT  || \
				     s->state == TCP_SYN_RECV) && s->is_acceptable_ack(p)) {
					ts_ecr -= s->ts_offset;	
					SET_TCP_RTT_ANNO(p, MAX(1, now - ts_ecr));
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
TCPAckOptionsParse::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPAckOptionsParse::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPAckOptionsParse)
