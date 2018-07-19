/*
 * tcpbuffer.{cc,hh} -- buffer TCP packets in sequence-number order
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

#ifndef CLICK_TCPBUFFER_HH
#define CLICK_TCPBUFFER_HH
#include <click/packet.hh>
#include <click/string.hh>
#include <click/vector.hh>
#include <clicknet/tcp.h>
#include "pktqueue.hh"
#include "tcpsack.hh"
CLICK_DECLS

class TCPBuffer : private PktQueue { public:

	TCPBuffer();
	~TCPBuffer();

	inline uint32_t bytes(void) const;
	inline uint32_t packets(void) const;
	inline bool empty(void) const;
	inline Packet *front(void) const;
	inline Packet *back(void) const;

	int insert(Packet *);
	bool peek(uint32_t);
	Packet *remove(uint32_t);
	TCPSack sack() const;
	String unparse() const;

  protected:

	Packet *_last;

};

inline uint32_t
TCPBuffer::bytes(void) const
{
	return PktQueue::bytes();
}

inline uint32_t
TCPBuffer::packets(void) const
{
	return PktQueue::packets();
}

inline bool
TCPBuffer::empty(void) const
{
	return PktQueue::empty();
}

inline Packet *
TCPBuffer::front(void) const
{
	return PktQueue::front();
}

inline Packet *
TCPBuffer::back(void) const
{
	return PktQueue::back();
}

CLICK_ENDDECLS
#endif
