/*
 * circularbuffer.hh -- circular buffer template
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

#ifndef CLICK_CIRCULARBUFFER_HH
#define CLICK_CIRCULARBUFFER_HH
#include <click/vector.hh>
CLICK_DECLS

#define CLICK_DEBUG_CB 0
#if CLICK_DEBUG_CB
# define cb_assert(x) assert(x)
#else
# define cb_assert(x)
#endif

template <typename T> 
class CircularBuffer { public:	

	typedef T value_type;
	typedef size_t size_type;

	/** @brief Construct an empty circular buffer. */
	CircularBuffer() {
		_buffer.resize(16, T());
		_capacity = 16;
		_head = _tail = _size = 0;
		_empty_value = T();
	}

	/** @brief Construct a circular buffer for @a n elements. */
	CircularBuffer(size_type n) {
		cb_assert(n > 0);

		_buffer.resize(n, T());
		_capacity = n;
		_head = _tail = _size = 0;
		_empty_value = T();
	}

	/** @brief Construct a circular buffer with @a n elements equal to @a v. */
	CircularBuffer(size_type n, const T &v) {
		cb_assert(n > 0);

		_buffer.resize(n, v);
		_capacity = n;
		_head = _tail = _size = 0;
		_empty_value = v;
	}

	/** @brief Construct a circular buffer as a copy of @a b. */
	CircularBuffer(const CircularBuffer<T> &b) {
		_buffer = b._buffer;
		_capacity = b._capacity;
		_head = b._head;
		_tail = b._tail;
		_size = b._size;
		_empty_value = b._empty_value;
	}

	/** @brief Return the first element in the buffer. 
	 * @pre !empty() */
	inline T &front() {
		cb_assert(!empty());
		return _buffer[_head];
	}
	/** @overload */
	inline const T &front() const {
		cb_assert(!empty());
		return _buffer[_head];
	}

	/** @brief Return the last element in the buffer. 
	 * @pre !empty() */
	inline T &back() {
		cb_assert(!empty());
		return _buffer[_tail];
	}
	/** @overload */
	inline const T &back() const {
		cb_assert(!empty());
		return _buffer[_tail];
	}

	/** @brief Insert a new element at the beginning of the buffer.
	 * @param x new element */
	inline void push_front(const T &x) {
		cb_assert(size() < capacity());

		if (!empty())	
			_head = (_head ? _head - 1 : _capacity - 1);

		_buffer[_head] = x;
		_size++;
	}

	/** @brief Insert a new element at the end of the buffer. 
	 * @param x new element */
	inline void push_back(const T &x) {
		cb_assert(size() < capacity());

		if (!empty())
			_tail = (_tail + 1 < _capacity ? _tail + 1 : 0);

		_buffer[_tail] = x;
		_size++;
	}

	/** @brief Remove the element at the beginning of the buffer.
	 * @pre !empty() */
	inline void pop_front() {
		cb_assert(!empty());

		_buffer[_head].~T();
		_buffer[_head] = _empty_value;
		_size--;

		if (!empty())
  			_head = (_head + 1 < _capacity ? _head + 1 : 0);
	}
	
	/** @brief Remove the element at the end of the buffer.
	 * @pre !empty() */
	inline void pop_back() {
		cb_assert(!empty());

		_buffer[_tail].~T();
		_buffer[_tail] = _empty_value;
		_size--;

		if (!empty())
			_tail = (_tail ? _tail - 1 : _capacity - 1);
	}

	/** @brief Return true iff size() == 0. */
	inline bool empty() {
		return size() == 0;
	}

	/** @brief Return the number of elements in the buffer. */
	inline size_type size() {
		return _size;
	}

	/** @brief Return the buffer capacity. */
	inline size_type capacity() {
		return _capacity;
	}

	/** @brief Remove all elements.
	 * @post size() == 0 */
	inline void clear() {
		_buffer.assign(_capacity, _empty_value);
		_head = _tail = _size = 0;
	}

	/** @brief Replace this buffer's contents with a copy of @a b. */
	inline CircularBuffer<T> &operator=(const CircularBuffer<T> &b) {
		if (likely(&b != this)) {
			_buffer = b._buffer;
			_capacity = b._capacity;
			_head = b._head;
			_tail = b._tail;
			_size = b._size;
			_empty_value = b._empty_value;
		}
		return *this;
	}
	
  private:

	T _empty_value;       // Empty element
	size_type _head;      // First element
	size_type _tail;      // Last element
	size_type _size;      // Buffer size
	size_type _capacity;  // Buffer capacity
	Vector<T> _buffer;    // Actual buffer
	
};

CLICK_ENDDECLS
#endif

