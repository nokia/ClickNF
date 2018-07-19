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

#ifndef CLICK_TCPSACK_HH
#define CLICK_TCPSACK_HH
#include <click/packet.hh>
#include <click/vector.hh>
#include <clicknet/tcp.hh>
CLICK_DECLS

class TCPSackBlock { public:
	inline TCPSackBlock(uint32_t, uint32_t);

	inline uint32_t left() const;
	inline uint32_t right() const;
	inline uint32_t length() const;

  protected:

	uint32_t _left;
	uint32_t _right;
};

inline
TCPSackBlock::TCPSackBlock(uint32_t l, uint32_t r)
	: _left(l), _right(r)
{
}

inline uint32_t 
TCPSackBlock::left() const
{
	return _left;
}

inline uint32_t 
TCPSackBlock::right() const
{
	return _right;
}

inline uint32_t 
TCPSackBlock::length() const
{
	return _right - _left;
}

struct click_tcp_sack {
	uint8_t opcode;
	uint8_t opsize;
	TCPSackBlock block[0];
};

class TCPSack { public:
	TCPSack();
	~TCPSack();

	inline void clear();
	inline bool empty() const;
	inline size_t blocks() const;

	int insert_block(const TCPSackBlock &);
	int remove_block(const TCPSackBlock &);

	static WritablePacket *insert_data(Packet *, uint8_t *&, uint16_t);
	static WritablePacket *remove_data(Packet *, uint8_t *&, uint16_t);
	static WritablePacket *insert_blocks(Packet *, click_tcp_sack *&, uint8_t);
	static WritablePacket *remove_blocks(Packet *, click_tcp_sack *&, uint8_t);

	inline TCPSackBlock operator[](size_t i) const;

  protected:

	Vector<TCPSackBlock> _block;
};

inline void
TCPSack::clear()
{
	_block.clear();
}

inline bool
TCPSack::empty() const
{
	return _block.empty();
}

inline size_t
TCPSack::blocks() const
{
	return _block.size();
}

inline TCPSackBlock
TCPSack::operator[](size_t i) const
{
	click_assert(i < blocks());
	return _block[i];
}

CLICK_ENDDECLS
#endif
