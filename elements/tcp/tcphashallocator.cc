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

/*
 * Copyright (c) 2008 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include "tcphashallocator.hh"
#include <click/integers.hh>
CLICK_DECLS

TCPHashAllocator::TCPHashAllocator(size_t size)
    : _free(0), _buffer(0), _size(size)
{
#ifdef VALGRIND_CREATE_MEMPOOL
    VALGRIND_CREATE_MEMPOOL(this, 0, 0);
#endif
    
    min_buffer_size = 10240;
#ifdef CLICK_LINUXMODULE
    max_buffer_size = 16384;
#elseif HAVE_HUGEPAGES //If hugepages, each buffer is a huge page
    min_buffer_size = getpagesizes(NULL, 0) * (1<<20);
    max_buffer_size = min_buffer_size;
#else
    max_buffer_size = 1048576;
#endif
    min_nelements = min_buffer_size / _size;

    assert(min_nelements > 0);
}

TCPHashAllocator::~TCPHashAllocator()
{
    while (buffer *b = _buffer) {
	_buffer = b->next;
#if HAVE_HUGEPAGES
	free_huge_pages(b);
#else
	delete[] reinterpret_cast<char *>(b);
#endif
	
    }
#ifdef VALGRIND_DESTROY_MEMPOOL
    VALGRIND_DESTROY_MEMPOOL(this);
#endif
}

void *TCPHashAllocator::hard_allocate()
{
    size_t nelements;

    if (!_buffer)
	nelements = (min_buffer_size - sizeof(buffer)) / _size;
    else {
	size_t shift = sizeof(size_t) * 8 - ffs_msb(_buffer->maxpos + _size);
	size_t new_size = 1 << (shift + 1);
	if (new_size > max_buffer_size)
	    new_size = max_buffer_size;
	nelements = (new_size - sizeof(buffer)) / _size;
    }
    if (nelements < min_nelements)
	nelements = min_nelements;
#if HAVE_HUGEPAGES
    buffer *b = reinterpret_cast<buffer *>(get_huge_pages(min_buffer_size, 0));
#else 
    buffer *b = reinterpret_cast<buffer *>(new char[sizeof(buffer) + _size * nelements]);
#endif
    
    if (b) {
	b->next = _buffer;
	_buffer = b;
	b->maxpos = sizeof(buffer) + _size * nelements;
	b->pos = sizeof(buffer) + _size;
	void *data = reinterpret_cast<char *>(_buffer) + sizeof(buffer);
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, data, _size);
#endif
	return data;
    } else
	return 0;
}

void TCPHashAllocator::swap(TCPHashAllocator &x)
{
    size_t xsize = _size;
    _size = x._size;
    x._size = xsize;

    link *xfree = _free;
    _free = x._free;
    x._free = xfree;

    buffer *xbuffer = _buffer;
    _buffer = x._buffer;
    x._buffer = xbuffer;

#ifdef VALGRIND_MOVE_MEMPOOL
    VALGRIND_MOVE_MEMPOOL(this, reinterpret_cast<TCPHashAllocator *>(100));
    VALGRIND_MOVE_MEMPOOL(&x, this);
    VALGRIND_MOVE_MEMPOOL(reinterpret_cast<TCPHashAllocator *>(100), &x);
#endif
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPHashAllocator)
