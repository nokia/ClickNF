/*
 * tcpsack.{cc,hh} -- TCP SACK information
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
#include <click/packet.hh>
#include <click/machine.hh>
#include <clicknet/tcp.h>
#include "tcpsack.hh"
CLICK_DECLS

TCPSack::TCPSack()
{
}

TCPSack::~TCPSack()
{
}

int
TCPSack::insert_block(const TCPSackBlock &b)
{
	// Make sure block is valid
	if (b.length() == 0)
		return -EINVAL;

	uint32_t l = b.left();
	uint32_t r = b.right();

	// Make sure it does not overlap
	for (size_t i = 0; i < blocks(); i++)
		if ((SEQ_LEQ(_block[i].left(), l) && SEQ_LT(l, _block[i].right())) || \
		    (SEQ_LT(_block[i].left(), r) && SEQ_LEQ(r, _block[i].right())))
			return -EEXIST;

	_block.push_back(b);
	return 0;
}

int 
TCPSack::remove_block(const TCPSackBlock &b)
{
	uint32_t l = b.left();
	uint32_t r = b.right();

	for (size_t i = 0; i < blocks(); i++) {
		if (l == _block[i].left() && r == _block[i].right()) {
			_block.erase(_block.begin() + i);
			return 0;
		}
	}

	return -EINVAL;
}

WritablePacket *
TCPSack::remove_data(Packet *p, uint8_t *& begin, uint16_t len)
{
	uint8_t *end = begin + len;

	click_assert(len > 0);
	click_assert(begin >= p->data() && begin < p->end_data());
	click_assert(end > p->data() && end <= p->end_data());

	WritablePacket *wp = p->uniqueify();
	if (!wp)
		return NULL;

	// Data length before
	uint16_t blen = (uint16_t)(begin - wp->data());

	// Data length after
	uint16_t alen = (uint16_t)(wp->end_data() - end);

	// Data length to remove
	uint16_t rlen = end - begin;

	// Adjust packet by copying the minimum amount of data
	if (blen < alen) {
		uint8_t *src = wp->data();
		uint8_t *dst = src + rlen;
		memmove(dst, src, blen);
		wp->pull(rlen);
	}
	else {
		uint8_t *dst = begin;
		uint8_t *src = end;
		memmove(dst, src, alen);
		wp->take(rlen);
	}

	// Update the initial pointer
	begin = wp->data() + blen;

	return wp;
}

WritablePacket *
TCPSack::insert_data(Packet *p, uint8_t *& begin, uint16_t len)
{
	click_assert(begin >= p->data() && begin < p->end_data());

	WritablePacket *wp = p->uniqueify();
	if (!wp)
		return NULL;

	// Data length before
	uint16_t blen = (uint16_t)(begin - wp->data());

	// Data length after
	uint16_t alen = (uint16_t)(wp->end_data() - begin);

	// Adjust packet by copying the minimum amount of data
	if (blen < alen) {
		wp = wp->push(len);
		if (!wp)
			return NULL;
		uint8_t *dst = wp->data();
		uint8_t *src = dst + len;
		memmove(dst, src, blen);
	}
	else {
		wp = wp->put(len);
		if (!wp)
			return NULL;
		uint8_t *src = begin;
		uint8_t *dst = src + len;
		memmove(dst, src, alen);
	}

	// Update the initial pointer
	begin = wp->data() + blen;

	return wp;
}

WritablePacket *
TCPSack::insert_blocks(Packet *p, click_tcp_sack *&sack, uint8_t b)
{
	click_assert(((sack->opsize - 2) >> 3) + b <= 4);

	uint8_t *end_sack = (uint8_t *)sack + sack->opsize;
	uint16_t len = (b << 3);

	WritablePacket *wp = insert_data(p, end_sack, len);
	if (!wp)
		return NULL;

	// Fix IP/TCP headers
	wp->set_ip_header((click_ip *)wp->data(), sizeof(click_ip));

	// Get headers
	click_ip *ip = wp->ip_header();
	click_tcp *th = wp->tcp_header();

	// Fix IP packet length and TCP offset
	ip->ip_len = htons(ntohs(ip->ip_len) + len);
	th->th_off += (len >> 2);

	// Update SACK pointer
	sack = (click_tcp_sack *)(end_sack - sack->opsize);

	// Update SACK size
	sack->opsize += len;

	return wp;
}

WritablePacket *
TCPSack::remove_blocks(Packet *p, click_tcp_sack *&sack, uint8_t b)
{
	uint8_t blocks = ((sack->opsize - 2) >> 3);
	click_assert(blocks >= b);

	uint8_t *begin;
	uint16_t len;

	if (blocks == b) {
		begin = (uint8_t *)sack - 2; // Include the 2 preceding NOPs
		len = sack->opsize + 2;      // Remove the entire option and NOPs
	}
	else {
		begin = (uint8_t *)&sack->block[blocks - b];
		len = (b << 3);
	}

	WritablePacket *wp = remove_data(p, begin, len);
	if (!wp)
		return NULL;

	// Fix IP/TCP headers
	wp->set_ip_header((click_ip *)wp->data(), sizeof(click_ip));

	// Get headers
	click_ip *ip = wp->ip_header();
	click_tcp *th = wp->tcp_header();

	// Fix IP packet length and TCP offset
	ip->ip_len = htons(ntohs(ip->ip_len) - len);
	th->th_off -= (len >> 2);

	// Update SACK
	if (blocks == b)
		sack = (click_tcp_sack *)begin;
	else {
		sack = (click_tcp_sack *)(begin - ((blocks - b) << 3) - 2);
		sack->opsize -= len;
	}

	return wp;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPSack)
