/*
 * tcpoptionsparser.{cc,hh} -- Parse TCP options
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
#include <clicknet/tcp.h>
#include <click/timestamp.hh>
#include "tcpinfo.hh"
#include "tcpstate.hh"
#include "tcpoptionsparser.hh"
#include "util.hh"
CLICK_DECLS

TCPOptionsParser::TCPOptionsParser()
{
}

void
TCPOptionsParser::handle_syn(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);

	// Reset RTT annotation
	SET_TCP_RTT_ANNO(p, 0);

	const click_tcp *th = p->tcp_header();

	// If no options or no state, return
	if (!s || th->th_off <= 5) {
		output(TCP_OPP_OUT_CCO_PORT).push(p);
		return;
	}

	bool ack = (th->th_flags & TH_ACK);
	click_assert(TCP_SYN(th) && !TCP_RST(th) && !TCP_FIN(th));

	// Option headers
	const uint8_t *ptr = (const uint8_t *)(th + 1);
	const uint8_t *end = (const uint8_t *)th + (th->th_off << 2);

	// Process each option
	while (ptr < end) {
		uint8_t opcode = ptr[0];

		if (opcode == TCPOPT_EOL)
			break;

		if (opcode == TCPOPT_NOP) {
			ptr++;
			continue;
		}

		// Stop if option is malformed
		if (ptr + 1 == end || ptr + ptr[1] > end)
			break;

		uint8_t opsize = ptr[1];

		switch (opcode) {
		case TCPOPT_MAXSEG:           // MSS
			if (opsize == TCPOLEN_MAXSEG) {
				uint16_t mss = ntohs(*(const uint16_t *)&ptr[2]);
				s->snd_mss = MIN(mss, TCP_RCV_MSS_DEFAULT);
			}
			break;

		case TCPOPT_WSCALE:           // Window scaling
			if (opsize == TCPOLEN_WSCALE) {
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
			if (opsize == TCPOLEN_SACK_PERMITTED)
				s->snd_sack_permitted = true;
			break;

		case TCPOPT_TIMESTAMP:        // Timestamp
			if (opsize == TCPOLEN_TIMESTAMP) {
				// Get now, preferably from the packet timestamp
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

				// Update RTT estimate
				if (ack) {
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

	output(TCP_OPP_OUT_CCO_PORT).push(p);
}

void
TCPOptionsParser::handle_ack(Packet *p)
{
	const click_tcp *th = p->tcp_header();
	TCPState *s = TCP_STATE_ANNO(p);

	// Reset RTT annotation
	SET_TCP_RTT_ANNO(p, 0);

	// If no options or no state, return
	if (!s || th->th_off <= 5) {
		output(TCP_OPP_OUT_CSN_PORT).push(p);
		return;
	}

//	bool syn = (th->th_flags & TH_SYN);
	bool rst = (th->th_flags & TH_RST);
//	bool ack = (th->th_flags & TH_ACK);
//	click_assert(!syn && ack);

	// Option headers
	const uint8_t *ptr = (const uint8_t *)(th + 1);
	const uint8_t *end = (const uint8_t *)th + (th->th_off << 2);

	// Process each option
	while (ptr < end) {
		uint8_t opcode = ptr[0];

		if (opcode == TCPOPT_EOL)
			break;

		if (opcode == TCPOPT_NOP) {
			ptr++;
			continue;
		}

		// Stop if option is malformed
		if (ptr + 1 == end || ptr + ptr[1] > end)
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
					const click_ip *qip = q->ip_header();
					const click_tcp *qth = q->tcp_header();
					uint32_t qseq = TCP_SEQ(qth);
					uint32_t qend = TCP_END(qip, qth);

					for (uint8_t i = 0; i < blocks; i++) {
						// First and last sequence numbers of block
						uint32_t bseq = ntohl(*(const uint32_t *)&ptr[2+8*i]);
						uint32_t bend = ntohl(*(const uint32_t *)&ptr[6+8*i])-1;

						if (SEQ_LEQ(bseq, qseq) && SEQ_LEQ(qend, bend)) {
							SET_TCP_SACK_FLAG_ANNO(p);
							break;
						}
					}

					q = q->next();

				} while (q != s->rtxq.front());
			}
			break;

		case TCPOPT_TIMESTAMP:
			if (opsize == TCPOLEN_TIMESTAMP && s->snd_ts_ok) {
				// Get now, preferably from the packet timestamp
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
				if (SEQ_LT(ts_val, s->ts_recent) && !rst) {
					uint32_t twenty_four_days_ms = 24 * 24 * 60 * 1000;
					if (SEQ_GT(now, s->ts_recent_update + twenty_four_days_ms)){
						s->ts_recent = ts_val;
						s->ts_recent_update = now;
					}
					else {
						s->ts_last_ack_sent = s->rcv_nxt;
						output(TCP_OPP_OUT_ACK_PORT).push(p);  // TCPAcker
						return;
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
				// Also allow CLOSE_WAIT state
				if ((s->state == TCP_ESTABLISHED || \
				     s->state == TCP_CLOSE_WAIT) && s->is_acceptable_ack(p)) {
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

	output(TCP_OPP_OUT_CSN_PORT).push(p);
}

void
TCPOptionsParser::push(int port, Packet *p)
{
	switch (port) {
	case TCP_OPP_IN_ACK_PORT:
		handle_ack(p);
		break;
	case TCP_OPP_IN_SYN_PORT:
		handle_syn(p);
		break;
	}
}

//Packet *
//TCPOptionsParser::pull(int port)
//{
//	if (port != TCP_OPP_IN_ACK_PORT)
//		return NULL;
//
//	Packet *p = input(0).pull();
//	if (p)
//		handle_ack(p);
//
//	return p;
//}


CLICK_ENDDECLS
EXPORT_ELEMENT(TCPOptionsParser)
