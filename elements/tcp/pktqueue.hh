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

#ifndef CLICK_PKTQUEUE_HH
#define CLICK_PKTQUEUE_HH
#include <click/packet.hh>
CLICK_DECLS

class PktQueue { public:
	PktQueue();
	~PktQueue();

	inline uint32_t bytes(void) const;
	inline uint32_t packets(void) const;
	inline uint32_t size(void) const;
	inline bool empty(void) const;
	inline Packet *front(void) const;
	inline Packet *back(void) const;
	void pull_front(uint32_t);
	void push_back(Packet *);
	void push_front(Packet *);
	void insert_after(Packet *, Packet *);
	void insert_before(Packet *, Packet *);
	void replace(Packet *, Packet *);
	void pop_front();
	void flush();

  protected:

	Packet *_head;
	uint32_t _bytes;
	uint32_t _packets;

};
	
inline uint32_t
PktQueue::bytes(void) const
{
	return _bytes;
}   

inline uint32_t
PktQueue::packets(void) const
{
	return _packets;
}   

inline uint32_t
PktQueue::size(void) const
{
	return packets();
}   

inline bool
PktQueue::empty(void) const
{
	return (_head == NULL);
}   

inline Packet *
PktQueue::front(void) const
{
	return _head;
}   

inline Packet *
PktQueue::back(void) const
{
	return _head ? _head->prev() : NULL;
}   

CLICK_ENDDECLS
#endif
