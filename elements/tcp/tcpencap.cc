/*
 * tcpencap.{cc,hh} -- encapsulates packet with a TCP header
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
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/tcpanno.hh>
#include "tcpencap.hh"
CLICK_DECLS

TCPEncap::TCPEncap()
{
}

int
TCPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_tsval  = 0;
	_tsecr  = 0;
	_seqno  = 0;
	_ackno  = 0;
	_flags  = 0;
	_window = 0;
	_urgent = 0;
	
	bool syn = false;
	bool ack = false;
	bool rst = false;
	bool fin = false;
	bool urg = false;
	bool psh = false;

	if (Args(conf, this, errh)
	    .read_mp("SRC", _src)
	    .read_mp("DST", _dst)
	    .read("SEQNO", _seqno)
	    .read("ACKNO", _ackno)
	    .read("SYN", syn)
	    .read("ACK", ack)
	    .read("RST", rst)
	    .read("FIN", fin)
	    .read("URG", urg)
	    .read("PSH", psh)
		.read("WINDOW", _window)
		.read("URGENT", _urgent)
		.read("TSVAL", _tsval)
		.read("TSECR", _tsval)
		.complete() < 0)
		return -1;

	_flags |= (syn ? TH_SYN  : 0);
	_flags |= (ack ? TH_ACK  : 0);
	_flags |= (rst ? TH_RST  : 0);
	_flags |= (fin ? TH_FIN  : 0);
	_flags |= (urg ? TH_URG  : 0);
	_flags |= (psh ? TH_PUSH : 0);

	return 0;
}


Packet *
TCPEncap::smaction(Packet *q)
{
	uint32_t len  = q->length();
	uint8_t oplen = (_tsval || _tsecr ? 12 : 0);

	// Make space for TCP header
	WritablePacket *p = q->push(sizeof(click_tcp) + oplen);
	if (!p)
		return NULL;

	// Set TCP header pointer
	click_tcp *th = reinterpret_cast<click_tcp *>(p->data());

	// TCP header
	th->th_sport  = htons(_src);
	th->th_dport  = htons(_dst);
	th->th_seq    = htonl(_seqno);
	th->th_ack    = htonl(_ackno);
	th->th_off    = (sizeof(click_tcp) + oplen) >> 2;
	th->th_flags2 = 0;
	th->th_flags  = _flags;
	th->th_win    = htons(_window);
	th->th_sum    = 0;
	th->th_urp    = htons(_urgent);

	if (_tsval || _tsecr) {
		uint8_t *ptr = (uint8_t *)(th + 1);

		// TCP timestamp
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_TIMESTAMP;
		ptr[3] = TCPOLEN_TIMESTAMP;

		// Pointers to the timestamps
		uint32_t *tsval = (uint32_t *)(ptr + 4);
		uint32_t *tsecr = (uint32_t *)(ptr + 8);

		*tsval = htonl(_tsval);
		*tsecr = htonl(_tsecr);
		_tsval++;
		_tsecr++;
	}

	_seqno += len;
	_ackno++;

	return p;
}

void
TCPEncap::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPEncap::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPEncap)
