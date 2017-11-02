/*
 * TCPTable.hh -- a per-process buffer of resources to use (i.e., fd, epfd)
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

#ifndef CLICK_TCPTABLE_HH
#define CLICK_TCPTABLE_HH
#include <click/vector.hh>
CLICK_DECLS

template <typename T>
class TCPTable { public:
	
	typedef T value_type;
	typedef size_t size_type;

	TCPTable() {};

	TCPTable(uint64_t x, uint64_t y,  const T &ini) {
		_table = Vector<Vector<T> >(x, Vector<T>(y, ini));
	}
	
	inline Vector<T> &operator[](size_type i) {
		assert(i < (size_type)_table.size());
		return _table[i];
	}
	
	inline  Vector<Vector<T> > &operator=(Vector<Vector<T> > v) {
		assert(v.size() == _table.size());
		_table = v;
		return _table;
	}
	
  private:

	Vector<Vector<T> > _table;

} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);

CLICK_ENDDECLS
#endif
