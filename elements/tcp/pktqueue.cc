/*
 * pktqueue.{cc,hh} -- simple packet queue
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
#include "pktqueue.hh"
#define CLICK_DEBUG_PKTQUEUE 0
#if CLICK_DEBUG_PKTQUEUE
# define click_assert(x) assert(x)
#else
# define click_assert(x) ((void)(0))
#endif
CLICK_DECLS

PktQueue::PktQueue()
	: _head(NULL), _bytes(0), _packets(0)
{
}

PktQueue::~PktQueue()
{
	flush();
}

void
PktQueue::insert_after(Packet *x, Packet *p)
{
	click_assert(x);
	Packet *n = x->next();

	p->set_prev(x);
	p->set_next(n);

	n->set_prev(p);
	x->set_next(p);

	_bytes += p->length();
	_packets++;
}

void 
PktQueue::insert_before(Packet *x, Packet *p)
{
	click_assert(x);
	insert_after(x->prev(), p);
}

void
PktQueue::pull_front(uint32_t len)
{
	click_assert(!empty());
	front()->pull(len);
	_bytes -= len;
}

void
PktQueue::push_back(Packet *p)
{
	if (_head == NULL) {
		p->set_next(p);
		p->set_prev(p);
		_head = p;
		_bytes = p->length();
		_packets = 1;
		return;
	}

	insert_after(_head->prev(), p);
}

void
PktQueue::push_front(Packet *p)
{
	push_back(p);
	_head = p;
}

void
PktQueue::replace(Packet *x, Packet *y)
{
	click_assert(x && y);
	Packet *n = x->next();
	Packet *p = x->prev();

	y->set_prev(p);
	y->set_next(n);

	n->set_prev(y);
	p->set_next(y);

	_bytes += y->length() - x->length();
}

void
PktQueue::pop_front()
{
	click_assert(!empty());

	Packet *p = _head;
	uint32_t len = p->length();
	Packet *next = p->next();
	Packet *prev = p->prev();
	p->set_next(NULL);
	p->set_prev(NULL);

	if (next == p && prev == p)
		_head = NULL;
	else {
		next->set_prev(prev);
		prev->set_next(next);
		_head = next;
	}
	_bytes -= len;
	_packets--;
}

void
PktQueue::flush()
{
	while (Packet *p = front()) {
		pop_front();
		p->kill();
	}
	click_assert(!_head && !_bytes && !_packets);
}

CLICK_ENDDECLS
#undef click_assert
ELEMENT_PROVIDES(PktQueue)
