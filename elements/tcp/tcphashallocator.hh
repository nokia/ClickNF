/*
 * tcphashallocator.{cc,hh} -- slight modification of Click hash allocator that can be used to allocate from hugepages
 * Massimo Gallo
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

#ifndef CLICK_TCPHASHALLOCATOR_HH
#define CLICK_TCPHASHALLOCATOR_HH
#if HAVE_VALGRIND && HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
#endif
#if HAVE_HUGEPAGES
extern "C" {
#include <hugetlbfs.h>
}
#endif


CLICK_DECLS

class TCPHashAllocator { public:

    TCPHashAllocator(size_t size);
    ~TCPHashAllocator();

    inline void increase_size(size_t new_size) {
	assert(!_free && !_buffer && new_size >= _size);
	_size = new_size;
    }

    inline void *allocate();
    inline void deallocate(void *p);

    void swap(TCPHashAllocator &x);

//  private:
  protected:

    struct link {
	link *next;
    };

    struct buffer {
	buffer *next;
	size_t pos;
	size_t maxpos;
    } CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);



    link *_free;
    buffer *_buffer;
    size_t _size;
    unsigned int min_buffer_size;
    unsigned int max_buffer_size;
    unsigned int min_nelements; 

    void *hard_allocate();

    TCPHashAllocator(const TCPHashAllocator &x);
    TCPHashAllocator &operator=(const TCPHashAllocator &x);

};


template <size_t size>
class SizedTCPHashAllocator : public TCPHashAllocator { public:

    SizedTCPHashAllocator()
	: TCPHashAllocator(size) {
    }

};


inline void *TCPHashAllocator::allocate()
{
    if (link *l = _free) {
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, l, _size);
	VALGRIND_MAKE_MEM_DEFINED(&l->next, sizeof(l->next));
#endif
	_free = l->next;
#ifdef VALGRIND_MAKE_MEM_DEFINED
	VALGRIND_MAKE_MEM_UNDEFINED(&l->next, sizeof(l->next));
#endif
	return l;
    } else if (_buffer && _buffer->pos < _buffer->maxpos) {
	void *data = reinterpret_cast<char *>(_buffer) + _buffer->pos;
	_buffer->pos += _size;
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, data, _size);
#endif
	return data;
    } else
	return hard_allocate();
}

inline void TCPHashAllocator::deallocate(void *p)
{
    if (p) {
	reinterpret_cast<link *>(p)->next = _free;
	_free = reinterpret_cast<link *>(p);
#ifdef VALGRIND_MEMPOOL_FREE
	VALGRIND_MEMPOOL_FREE(this, p);
#endif
    }
}

CLICK_ENDDECLS
#endif
