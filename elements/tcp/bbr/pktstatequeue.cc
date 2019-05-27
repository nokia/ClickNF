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
#include <click/machine.hh>

#include "pktstatequeue.hh"
#define CLICK_DEBUG_PKTSTATEQUEUE 0
#if CLICK_DEBUG_PKTSTTEQUEUE
# define click_assert(x) assert(x)
#else
# define click_assert(x) ((void)(0))
#endif
CLICK_DECLS

PktStateQueue::PktStateQueue() :
		_head(NULL), _size(0) {
}

PktStateQueue::~PktStateQueue() {
	flush();
}

void PktStateQueue::insert_after(pkt_state*& x, pkt_state* p) {
	click_assert(x);
	pkt_state *n = x->next;
	p->prev = x;
	p->next = n;
	x->next = p;
	n->prev = p;

	_size++;
}

void PktStateQueue::insert_before(struct pkt_state *x, struct pkt_state *p) {
	click_assert(x);
	insert_after(x->prev, p);
}

void PktStateQueue::push_back(struct pkt_state* p) {
	if (_head == NULL) {
		p->next = p;
		p->prev = p;
		_head = p;
		_size = 1;
		return;
	}
	insert_after(_head->prev, p);
}

void PktStateQueue::push_front(struct pkt_state* p) {
	push_back(p);
	_head = p;
}

void PktStateQueue::replace(struct pkt_state *x, struct pkt_state *y) {
	click_assert(x && y);
	struct pkt_state *n = x->next;
	struct pkt_state *p = x->prev;

	y->prev = p;
	y->next = n;

	n->prev = y;
	p->next = y;

}

void PktStateQueue::pop_front() {
	if (!empty()) {
		pkt_state *p = _head;
		pkt_state *next = p->next;
		pkt_state *prev = p->prev;
		p->next = NULL;
		p->prev = NULL;

		if (next->end == p->end && prev->end == p->end)
			_head = NULL;
		else {
			next->prev = prev;
			prev->next = next;
			_head = next;
		}
		_size--;
	}
}

void PktStateQueue::flush() {
	while (front()) {
		pop_front();
	}
	click_assert(!_head && !_size);
}

CLICK_ENDDECLS
#undef click_assert
ELEMENT_PROVIDES(PktStateQueue)
