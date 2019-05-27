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

#ifndef CLICK_PKTSTATEQUEUE_HH
#define CLICK_PKTSTATEQUEUE_HH
#include <click/packet.hh>
CLICK_DECLS

struct pkt_state {
	uint32_t seq;
	uint32_t end;
	uint32_t delivered;
	uint64_t first_sent_time;
	uint64_t delivered_time;
	uint32_t app_limited;
	pkt_state *prev;
	pkt_state *next;
	pkt_state(uint32_t seq, uint32_t end, uint32_t delivered,
			uint32_t first_sent_time, uint32_t delivered_time,
			uint32_t app_limited, struct pkt_state *prev,
			struct pkt_state *next) {
		this->seq = seq;
		this->end = end;
		this->delivered = delivered;
		this->first_sent_time = first_sent_time;
		this->delivered_time = delivered_time;
		this->app_limited = app_limited;
		this->prev = prev;
		this->next = next;
	}
	pkt_state(const pkt_state& copy_from) {
		seq = copy_from.seq;
		end = copy_from.end;
		delivered = copy_from.delivered;
		first_sent_time = copy_from.first_sent_time;
		delivered_time = copy_from.delivered_time;
		app_limited = copy_from.app_limited;
		if (copy_from.prev)
			prev = new pkt_state(*copy_from.prev);
		else
			prev = NULL;
		if (copy_from.next)
			next = new pkt_state(*copy_from.next);
		else
			next = NULL;
	}
};
class PktStateQueue {
public:
	PktStateQueue();
	~PktStateQueue();

	inline uint32_t size(void) const;
	inline bool empty(void) const;
	inline struct pkt_state *front(void) const;
	inline struct pkt_state *back(void) const;
	void push_back(struct pkt_state*);
	void push_front(struct pkt_state*);
	void insert_after(struct pkt_state*&, struct pkt_state*);
	void insert_before(struct pkt_state*, struct pkt_state*);
	void replace(struct pkt_state*, struct pkt_state*);
	void pop_front();
	void flush();

protected:

	struct pkt_state* _head;

	uint32_t _size;

};

inline uint32_t PktStateQueue::size(void) const {
	return _size;
}

inline bool PktStateQueue::empty(void) const {
	return (_head == NULL);
}

inline struct pkt_state*
PktStateQueue::front(void) const {
	return _head;
}

inline struct pkt_state *
PktStateQueue::back(void) const {
	return _head ? _head->prev : NULL;
}

CLICK_ENDDECLS
#endif
