/*
 * tcpoptionsunparser.{cc,hh} -- Unparse TCP options
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpsack.hh"
#include "tcpstate.hh"
#include "tcpoptionsunparser.hh"
#include "util.hh"
CLICK_DECLS

TCPOptionsUnparser::TCPOptionsUnparser()
{
}

int
TCPOptionsUnparser::initialize(ErrorHandler *)
{
	return 0;
}


void
TCPOptionsUnparser::push(int port, Packet *p)
{
	switch (port) {
	case TCP_OPU_IN_SYN:
		handle_syn(p);
		break;
	case TCP_OPU_IN_ACK:
		handle_ack(p);
		break;
	case TCP_OPU_IN_RTX:
		handle_rtx(p);
		break;
	}
}

void
TCPOptionsUnparser::handle_syn(Packet *pp)
{
	TCPState *s = TCP_STATE_ANNO(pp);

	// If no state, return
	if (!s) {
		output(TCP_OPU_OUT_ENQ).push(pp);
		return;
	}

	WritablePacket *p = pp->uniqueify();
	assert(p);

	click_ip *ip = p->ip_header();
	click_tcp *th = p->tcp_header();

	bool syn = (th->th_flags & TH_SYN);
	bool ack = (th->th_flags & TH_ACK);

	uint8_t  th_off = th->th_off;
	uint16_t ip_len = ntohs(ip->ip_len);

	uint8_t *ptr = (uint8_t *)(th + 1);

	// Assume a TCP SYN packet with no options and no data
	assert(syn && th->th_off == 5 && TCP_LEN(ip, th) == 0);

	// Make sure the state is correct
	assert(s->state == TCP_SYN_SENT || s->state == TCP_SYN_RECV);

	// RFC 6691:
	// "The MSS value to be sent in an MSS option should be equal to the
	//  effective MTU minus the fixed IP and TCP headers.  By ignoring both
	//  IP and TCP options when calculating the value for the MSS option, if
	//  there are any IP or TCP options to be sent in a packet, then the
	//  sender must decrease the size of the TCP data accordingly."

	// Add space for MSS
	p = p->put(4);
	ip = p->ip_header();
	th = p->tcp_header();

	// Maximum segment size
	ptr[0] = TCPOPT_MAXSEG;
	ptr[1] = TCPOLEN_MAXSEG;
	ptr[2] = s->rcv_mss >> 8;
	ptr[3] = s->rcv_mss & 0xff;
	ptr += 4;

	th_off += 1;
	ip_len += 4;

	// Only add window scale in the SYN-ACK if we saw it in the SYN first
	if (!ack || s->snd_wscale_ok) {
		// Add space for window scale
		p = p->put(4);
		ip = p->ip_header();
		th = p->tcp_header();

		// Window scale
		ptr[0] = TCPOPT_WSCALE;
		ptr[1] = TCPOLEN_WSCALE;
//		ptr[2] = s->rcv_wscale_default;
		ptr[2] = TCP_RCV_WSCALE_DEFAULT;
		ptr[3] = TCPOPT_NOP;
		ptr += 4;

		th_off += 1;
		ip_len += 4;
	}

	// Only add timestamp in the SYN-ACK if we saw it in the SYN first
	if (!ack || s->snd_ts_ok) {

		// Sample random offset
		s->ts_offset = click_random(0, 0xFFFFFFFFU);

		// Get now, preferably from packet timestamp
		uint32_t now = (uint32_t)p->timestamp_anno().usecval();
		if (now == 0)
			now = (uint32_t)Timestamp::now_steady().usecval();

		// Pointers to the timestamps
		uint32_t *ts_val = (uint32_t *)(ptr + 4);
		uint32_t *ts_ecr = (uint32_t *)(ptr + 8);

		// Add space for timestamp option
		p = p->put(12);
		ip = p->ip_header();
		th = p->tcp_header();

		// TCP timestamp
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_TIMESTAMP;
		ptr[3] = TCPOLEN_TIMESTAMP;

		*ts_val = htonl(s->ts_offset + now);
		*ts_ecr = htonl(ack ? s->ts_recent : 0);

		ptr += 12;

		th_off +=  3;
		ip_len += 12;

		if (ack) {
			uint32_t ackno = TCP_ACK(th);
			if (SEQ_GT(ackno, s->ts_last_ack_sent))
				s->ts_last_ack_sent = ackno;
		}
	}

	// Only add SACK-PERMITTED in the SYN-ACK if we saw it in the SYN first
	if (!ack || s->snd_sack_permitted) {
		// Add space for SACK permitted
		p = p->put(4);
		ip = p->ip_header();
		th = p->tcp_header();

		// SACK permitted
		ptr[0] = TCPOPT_SACK_PERMITTED;
		ptr[1] = TCPOLEN_SACK_PERMITTED;
		ptr[2] = TCPOPT_NOP;
		ptr[3] = TCPOPT_NOP;
		ptr += 4;

		th_off += 1;
		ip_len += 4;
	}

	th->th_off = th_off;
	ip->ip_len = htons(ip_len);

	output(TCP_OPU_OUT_ENQ).push(p);
}

void
TCPOptionsUnparser::handle_ack(Packet *pp)
{
	TCPState *s = TCP_STATE_ANNO(pp);

	// If no state, return
	if (!s) {
		output(TCP_OPU_OUT_DPB).push(pp);
		return;
	}

	WritablePacket *p = pp->uniqueify();
	assert(p);

	click_ip *ip = p->ip_header();
	click_tcp *th = p->tcp_header();

	bool ack = (th->th_flags & TH_ACK);

	uint8_t  th_off = th->th_off;
	uint16_t ip_len = ntohs(ip->ip_len);

	uint8_t *ptr = (uint8_t *)(th + 1);

	// Assume a TCP packet with no options and no data
	assert(ack && th_off == 5 && TCP_LEN(ip, th) == 0);

	// TCP Timestamp
	if (s->snd_ts_ok) {
		// Get now, preferably from packet timestamp
		uint32_t now = (uint32_t)p->timestamp_anno().usecval();
		if (now == 0)
			now = (uint32_t)Timestamp::now_steady().usecval();

		// Pointers to the timestamps
		uint32_t *ts_val = (uint32_t *)(ptr + 4);
		uint32_t *ts_ecr = (uint32_t *)(ptr + 8);

		// Add space for timestamp option
		p = p->put(12);
		ip = p->ip_header();
		th = p->tcp_header();

		// TCP timestamp
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_TIMESTAMP;
		ptr[3] = TCPOLEN_TIMESTAMP;

		*ts_val = htonl(s->ts_offset + now);
		*ts_ecr = htonl(s->ts_recent);

		ptr += 12;

		th_off +=  3;
		ip_len += 12;

		uint32_t ackno = TCP_ACK(th);
		if (SEQ_GT(ackno, s->ts_last_ack_sent))
			s->ts_last_ack_sent = ackno;
	}

	// Selective ACK (SACK)
	if (s->snd_sack_permitted && !s->rxb.empty()) {
		TCPSack sack = s->rxb.sack();
		uint8_t max_blocks = (s->snd_ts_ok ? 3 : 4);
		uint8_t blocks = MIN(max_blocks, sack.blocks());

		// Add space for SACK option
		p = p->put(4 + 8*blocks);
		ip = p->ip_header();
		th = p->tcp_header();

		// TCP SACK
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_SACK;
		ptr[3] = 2 + 8*blocks;

		ptr += 4;

		for (int i = 0; i < blocks; i++) {
			uint32_t *l = (uint32_t *)(ptr + 0);
			uint32_t *r = (uint32_t *)(ptr + 4);

			*l = htonl(sack[i].left());
			*r = htonl(sack[i].right());

			ptr += 8;
		}

		th_off += 1 + 2*blocks;
		ip_len += 4 + 8*blocks;
	}

	th->th_off = th_off;
	ip->ip_len = htons(ip_len);

	output(TCP_OPU_OUT_DPB).push(p);
}

void
TCPOptionsUnparser::handle_rtx(Packet *pp)
{
	TCPState *s = TCP_STATE_ANNO(pp);

	const click_tcp *pth = pp->tcp_header();

	// If no state, return
	if (!s || pth->th_off <= 5) {
		output(TCP_OPU_OUT_OUT).push(pp);
		return;
	}

	WritablePacket *p = pp->uniqueify();
	assert(p);

	click_tcp *th = p->tcp_header();

	uint8_t *ptr = (uint8_t *)(th + 1);
	uint8_t *end = (uint8_t *)th + (th->th_off << 2);

	// Make sure the packet already has at least an option
	assert(ptr != end);

	// Just update the timestamp and SACK
	while (ptr < end) {
		uint8_t opcode = ptr[0];

		if (opcode == TCPOPT_EOL)
			break;

		if (opcode == TCPOPT_NOP) {
			ptr++;
			continue;
		}

		// Make sure that option is not malformed
		assert(ptr + 1 < end && ptr + ptr[1] <= end);

		uint8_t opsize = ptr[1];

		switch (opcode) {
		case TCPOPT_TIMESTAMP:
			if (opsize == TCPOLEN_TIMESTAMP) {
				uint32_t *ts_val = (uint32_t *)(ptr + 2);
				uint32_t *ts_ecr = (uint32_t *)(ptr + 6);

				// Get now, preferably from packet timestamp
				uint32_t now = (uint32_t)p->timestamp_anno().usecval();
				if (now == 0)
					now = (uint32_t)Timestamp::now_steady().usecval();

				// Fill timestamp parameters
				*ts_val = htonl(s->ts_offset + now);
				*ts_ecr = htonl(s->ts_recent);
			}
			break;

		case TCPOPT_SACK: {
			TCPSack sack = s->rxb.sack();
			uint8_t max_blocks = (s->snd_ts_ok ? 3 : 4);
			uint8_t blocks_to_insert = MIN(max_blocks, sack.blocks());
			uint8_t blocks_in_packet = ((opsize - 2) >> 3);

			if (blocks_to_insert != blocks_in_packet) {
				click_tcp_sack *sh = (click_tcp_sack *)ptr;
				if (blocks_to_insert > blocks_in_packet) {
					uint8_t blocks = (blocks_to_insert - blocks_in_packet);
					p = TCPSack::insert_blocks(p, sh, blocks);
				}
				else {
					uint8_t blocks = (blocks_in_packet - blocks_to_insert);
					p = TCPSack::remove_blocks(p, sh, blocks);
				}
				if (!p)
					return;

				th = p->tcp_header();
				ptr = (uint8_t *)sh;
				end = (uint8_t *)th + (th->th_off << 2);
				opsize = (blocks_to_insert > 0 ? sh->opsize : 0);
			}

			for (int i = 0; i < blocks_to_insert; i++) {
				uint32_t *l = (uint32_t *)&ptr[2 + 8*i];
				uint32_t *r = (uint32_t *)&ptr[6 + 8*i];

				*l = htonl(sack[i].left());
				*r = htonl(sack[i].right());
			}

			break;
		}
		default:
			break;
		}

		ptr += opsize;
	}

	output(TCP_OPU_OUT_OUT).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPOptionsUnparser)

