/*
 * tcplist.hh -- TCP list template
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

/*
 * list.hh -- List template
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software")
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */
#ifndef CLICK_TCPLIST_HH
#define CLICK_TCPLIST_HH

#include <click/glue.hh>
#define CLICK_DEBUG_TCPLIST 0
#if CLICK_DEBUG_TCPLIST
# define tcpl_assert(x) assert(x)
#else
# define tcpl_assert(x)
#endif
CLICK_DECLS

/** @file <click/list.hh>
 * @brief Click's doubly-linked list container template.
 */

class TCPList_member;
template <typename T, TCPList_member T::*member> class TCPList;

/** @class List
  @brief Doubly-linked list template.

  The List template, and its helper template TCPList_member, implement a generic
  doubly-linked list.  The list is <em>intrusive</em> in that the container
  does not manage space for its contents.  The user provides space for
  contained elements, and must delete elements when they are no longer needed.
  (This is unlike Vector or HashTable, which manage space for their contents.)
  The main advantage of intrusive containers is that a single element can be
  on multiple lists.

  Here's an example linked list of integers built using List and TCPList_member.

  @code
  #include "list.hh"

  struct intlist_node {
      int value;
      List_member<intlist_node> link;
      intlist_node(int v)
          : value(v) {
      }
  };

  typedef List<intlist_node, &intlist_node::link> intlist;

  void make_intlist(intlist &l, int begin, int end, int step) {
      for (int i = begin; i < end; i += step)
          l.push_back(new intlist_node(i));
      // Note that l does not manage its contents' memory!
      // Whoever destroys l should first delete its contents,
      // for example by calling trash_intlist(l).
  }

  void print_intlist(const intlist &l) {
      size_t n = 0;
      for (intlist::const_iterator it = l.begin(); it != l.end(); ++it, ++n)
          click_chatter("#%ld: %d\n", (long) n, it->value);
  }

  void trash_intlist(intlist &l) {
      while (!l.empty()) {
          intlist_node *n = l.front();
	  l.pop_front();
	  delete n;
      }
  }

  template <typename T>
  void remove_every_other(T &list) {
      typename T::iterator it = list.begin();
      while (it != l.end()) {
          ++it;
	  if (it != l.end())
	      it = list.erase(it);
      }
  }
  @endcode
*/

/** @class TCPList_member
  @brief Member of classes to be placed on a List.

  Any class type that will be placed on a List must have a publicly accessible
  TCPList_member member. This member is supplied as the second template argument
  to TCPList. TCPList_member allows users to fetch the next-element and
  previous-element pointers, but all modifications must take place via TCPList
  functions like TCPList::push_back() and  TCPList::insert().  TCPList_member has
  private copy constructor and default assignment operators.

  @sa List
*/
class TCPList_member { public:

	/** @brief Construct an isolated member. */
	TCPList_member()
		: _next(this), _prev(this) {
	}

	/** @brief Return the next member in the list. */
	inline TCPList_member *next() {
		return _next;
	}

	/** @overload */
	inline const TCPList_member *next() const {
		return _next;
	}

	/** @brief Return the previous member in the list. */
	inline TCPList_member *prev() {
		return _prev;
	}

	/** @overload */
	inline const TCPList_member *prev() const {
		return _prev;
	}

	/** @brief Return true iff (_next == this && _prev == this). */
	inline bool isolated() const {
		return (_next == this && _prev == this);
	}

	/** @brief Isolate member. */
	inline void isolate() {
		_next = this;
		_prev = this;
	}

	/** @brief Detach member. */
	inline void detach() {
		if (!isolated()) {
			// Remove member from list
			_prev->_next = _next;
			_next->_prev = _prev;

			isolate();
		}
	}

  private:

	TCPList_member *_next;
	TCPList_member *_prev;

	/** @brief Construct a member from @a x. */
	TCPList_member(const TCPList_member &x) {
		_next = x._next;
		_prev = x._prev;
	}

	/** @brief Assign @a x to this member. */
	TCPList_member &operator=(const TCPList_member &x) {
		if (likely(&x != this)) {
			_next = x._next;
			_prev = x._prev;
		}
		return *this;
	}

	template <typename X, TCPList_member X::*member> friend class TCPList;
};

template <typename T, TCPList_member T::*member>
class TCPList { public:

	typedef T *pointer;
	typedef const T *const_pointer;
	class const_iterator;
	class iterator;
	typedef size_t size_type;

	/** @brief Construct an empty list. */
	TCPList() : _head(), _size(0) {
	}

	/** @brief Destruct the list. */
	~TCPList() {
	}

	/** @brief Return an iterator for the first element in the list. */
	inline iterator begin() {
		return iterator(next(&_head), this);
	}
	/** @overload */
	inline const_iterator begin() const {
		return const_iterator(next(&_head), this);
	}
	/** @brief Return a const_iterator for the first element in the list. */
	inline const_iterator cbegin() const {
		return const_iterator(next(&_head), this);
	}
	/** @brief Return an iterator for the end of the list.
	 * @invariant end().live() == false */
	inline iterator end() {
		return iterator(this);
	}
	/** @overload */
	inline const_iterator end() const {
		return const_iterator(this);
	}
	/** @brief Return a const_iterator for the end of the list. */
	inline const_iterator cend() const {
		return const_iterator(this);
	}

	/** @brief Return true iff size() == 0. */
	inline bool empty() const {
		return size() == 0;
	}

    /** @brief Return the number of elements in the list. */
    size_type size() const {
		return _size;
    }

    /** @brief Test if an element @a e exists in the table. */
    inline bool contains(const T *e) const {
		for (const TCPList_member *m = _head.next(); m != &_head; m = m->next())
			if (e == container(m))
				return true;
        return false;
    }

	/** @brief Return the first element in the list.
	 *
	 * Returns a null pointer if the list is empty.*/
	inline T *front() {
		return next(&_head);
	}
	/** @overload */
	inline const T *front() const {
		return next(&_head);
	}
	/** @brief Return the last element in the list.
	 *
	 * Returns a null pointer if the list is empty. */
	inline T *back() {
		return prev(&_head);
	}
	/** @overload */
	inline const T *back() const {
		return prev(&_head);
	}

	/** @brief Insert a new element at the beginning of the list.
	 * @param x new element
	 * @pre @a x != NULL */
	inline void push_front(T *x) {
		tcpl_assert(x);
		insert(_head._next, &(x->*member));
	}

	/** @brief Insert a new element at the end of the list.
	 * @param x new element
	 * @pre @a x != NULL */
	inline void push_back(T *x) {
		tcpl_assert(x);
		insert(&_head, &(x->*member));
	}

	/** @brief Remove the element at the beginning of the list.
	 * @pre !empty() */
	inline void pop_front() {
		tcpl_assert(!empty());
		erase(_head._next);
	}

	/** @brief Remove the element at the end of the list.
	 * @pre !empty() */
	inline void pop_back() {
		tcpl_assert(!empty());
		erase(_head._prev);
	}

	/** @brief Insert an element before @a pos.
	 * @param pos position to insert (if null, insert at end of list)
	 * @param x new element
	 * @pre @a x != NULL && isolated(@a x) */
	inline void insert(T *pos, T *x) {
		tcpl_assert(x && isolated(x));
		if (pos)
			insert(&(pos->*member), &(x->*member));
		else
			push_back(x);
	}

	/** @brief Insert an element before @a it.
	 * @param it position to insert
	 * @param x new element
	 * @return an iterator pointing to @a x
	 * @pre @a x != NULL && isolated(@a x) */
	inline iterator insert(iterator it, T *x) {
		insert(it.get(), x);
		return iterator(x, this);
	}

	/** @brief Insert the elements in [@a first, @a last) before @a it.
	 * @param it position to insert
	 * @param first iterator to beginning of insertion sequence
	 * @param last iterator to end of insertion sequence
	 * @pre isolated(@a x) for each @a x in [@a first, @a last) */
	template <typename InputIterator>
	inline void insert(iterator it, InputIterator first, InputIterator last) {
		while (first != last) {
			insert(it, *first);
			++first;
		}
	}

	/** @brief Replace the element at @a pos with @a x.
	 * @param pos position to replace
	 * @param x new element
	 * @pre @a pos != NULL && @a x != NULL && isolated(@a x) */
	inline void replace(T *pos, T *x) {
		tcpl_assert(pos && !isolated(pos) && x && isolated(x));
		replace(&(pos->*member), &(x->*member));
	}

	/** @brief Remove @a x from the list.
	 * @param x element to remove
	 * @pre @a x != NULL && !isolated(@a x)*/
	//NOTE if x does not belong to this list, it will be removed from its list and TCP_list::size will be corrupted.
	inline void erase(T *x) {
		tcpl_assert(x && !isolated(x));
		erase(&(x->*member));
	}


	/** @brief Remove the element pointed to by @a it from the list.
	 * @param it element to remove
	 * @return iterator pointing to the element after the removed element
	 * @pre @a it.live() */
	inline iterator erase(iterator it) {
		tcpl_assert(it);
		iterator next = it + 1;
		erase(it.get());
		return next;
	}

	/** @brief Remove the elements in [@a first, @a last) from the list.
	 * @param first iterator to beginning of removal subsequence
	 * @param last iterator to end of removal subsequence
	 * @return iterator pointing to the element after the removed subsequence */
	inline iterator erase(iterator first, iterator last) {
		while (first != last)
			first = erase(first);
		return first;
	}

	/** @brief Remove all elements from the list.
	 * @note Equivalent to erase(begin(), end()). */
	inline void clear() {
		_size = 0;
		_head.isolate();
	}

	/** @brief Exchange list contents with list @a x. */
	inline void swap(TCPList<T, member> &x) {
		if (likely(&x != this)) {
			TCPList_member head(_head);
			size_type size(_size);

			_head = x._head;
			_size = x._size;
			x._head = head;
			x._size = size;
		}
	}

	/** @class TCPList::const_iterator
	 * @brief Const iterator type for TCPList. */
	class const_iterator { public:
		/** @brief Construct an end iterator for @a list. */
		const_iterator(const TCPList<T, member> *list)
			: _x(NULL), _list(list) {
			tcpl_assert(list);
		}
		/** @brief Construct an iterator from @a it. */
		const_iterator(const const_iterator &it) 
			: _x(it._x), _list(it._list) {
		}
		/** @brief Construct an iterator pointing at @a x in @a list. */
		const_iterator(const T *x, const TCPList<T, member> *list)
			: _x(const_cast<T *>(x)), _list(list) {
			tcpl_assert(list);
		}
		/** @brief Destroy the iterator. */
		~const_iterator() {
		}
		typedef bool (const_iterator::*unspecified_bool_type)() const;
		/** @brief Test if this iterator points to a valid list element. */
		inline operator unspecified_bool_type() const {
			return live() ? &const_iterator::live : NULL;
		}
		/** @brief Test if this iterator points to a valid list element. */
		inline bool live() const {
			return _x;// && _list;
		}
		/** @brief Return the current list element or null. */
		inline const T *get() const {
			return _x;
		}
		/** @brief Return the current list element or null. */
		inline const T *operator->() const {
			return _x;
		}
		/** @brief Return the current list element. */
		inline const T &operator*() const {
			return *_x;
		}
		/** @brief Advance this iterator to the next element. */
		inline void operator++() {
			if (!live())
				return;

			_x = _list->next(&(_x->*member));
		}
		/** @brief Advance this iterator to the next element. */
		inline void operator++(int) {
			++*this;
		}
		/** @brief Move this iterator forward by @a x positions.
		 * @return reference to this iterator
		 * @note This function takes O(abs(@a x)) time. */
		inline const_iterator &operator+=(int x) {
			tcpl_assert(x >= 0);
			for (; x > 0; --x)
				++*this;
			return *this;
		}
		/** @brief Return an iterator @a x positions ahead. */
		inline const_iterator operator+(int x) const {
			const_iterator it(*this);
			return it += x;
		}
		/** @brief Test if this iterator equals @a x. */
		inline bool operator==(const_iterator &x) const {
			return _x == x._x && _list == x._list;
		}
		/** @brief Test if this iterator does not equal @a x. */
		inline bool operator!=(const_iterator &x) const {
			return !(*this == x);
		}
		/** @brief Assign @a x to this iterator. */
		inline const_iterator &operator=(const const_iterator &x) {
			if (likely(&x != this)) {
				_x = x._x;
				_list = x._list;
			}
			return *this;
		}
	  private:
		T *_x;
		const TCPList<T, member> *_list;
		friend class iterator;
	};

	/** @class TCPList::iterator
	 * @brief Iterator type for TCPList. */
	class iterator : public const_iterator { public:
		/** @brief Construct an end iterator for @a list. */
		iterator(const TCPList<T, member> *list)
			: const_iterator(list) {
		}
		/** @brief Construct an iterator pointing at @a x in @a list. */
		iterator(T *x, const TCPList<T, member> *list)
			: const_iterator(x, list) {
		}
		/** @brief Return the current list element or null. */
		inline T *get() const {
			return this->_x;
		}
		/** @brief Return the current list element or null. */
		inline T *operator->() const {
			return this->_x;
		}
		/** @brief Return the current list element. */
		inline T &operator*() const {
			return *this->_x;
		}

		/** @brief Move this iterator forward by @a x positions.
		 * @return reference to this iterator
		 * @note This function takes O(abs(@a x)) time. */
		inline iterator &operator+=(int x) {
			tcpl_assert(x >= 0);
			for (; x > 0; --x)
				++*this;
			return *this;
		}
		/** @brief Return an iterator @a x positions ahead. */
		inline iterator operator+(int x) const {
			iterator it(*this);
			return it += x;
		}
	};

  private:
	/** @brief Check if @a x is isolated.
	 *
	 * An isolated element is not a member of any list. */
	inline bool isolated(const T *x) const {
		return (x->*member).isolated();
	}
	/** @brief Return a pointer to the container of member */
	static inline T *container(TCPList_member *m) {
		return (T *)((uint8_t *)m - (uint8_t *)&(((T *)NULL)->*member));
	}
	/** @overload */
	static inline const T *container(const TCPList_member *m) {
		return (const T*)((uint8_t *)m - (uint8_t *)&(((T *)NULL)->*member));
	}
	/** @brief Get the next element from member @a m. */
	inline T *next(TCPList_member *m) const {
		TCPList_member *n = m->next();
		return (n == &_head ? NULL : container(n));
	}
	/** @overload */
	inline const T *next(const TCPList_member *m) const {
		const TCPList_member *n = m->next();
		return (n == &_head ? NULL : container(n));
	}
	/** @brief Get the previous element from member @a m. */
	inline T *prev(TCPList_member *m) const {
		TCPList_member *n = m->prev();
		return (n == &_head ? NULL : container(n));
	}
	/** @overload */
	inline const T *prev(const TCPList_member *m) const {
		const TCPList_member *n = m->prev();
		return (n == &_head ? NULL : container(n));
	}
	/** @brief Insert a member before @a pos.
	 * @param pos position to insert
	 * @param m new member
	 * @pre @a pos != NULL && @a m != NULL */
	inline void insert(TCPList_member *pos, TCPList_member *m) {
		tcpl_assert(pos && m);

		m->_next = pos;
		m->_prev = pos->_prev;
		pos->_prev->_next = m;
		pos->_prev = m;
		_size++;
	}
	/** @brief Remove @a m from the list.
	 * @param m member to remove
	 * @pre (@a m != NULL) */
	inline void erase(TCPList_member *m) {
		tcpl_assert(m);

		_size--;
		m->_prev->_next = m->_next;
		m->_next->_prev = m->_prev;
	}
	/** @brief Replace member at @a pos with @a m.
	 * @param pos position of element to be replaced
	 * @param m new member
	 * @pre @a pos != NULL && @a m != NULL */
	inline void replace(TCPList_member *pos, TCPList_member *m) {
		tcpl_assert(pos && m);

		m->_next = pos->_next;
		m->_prev = pos->_prev;
		pos->_prev->_next = m;
		pos->_next->_prev = m;
	}

	TCPList_member _head;
	size_type _size;

};

CLICK_ENDDECLS
# undef tcpl_assert
#endif /* CLICK_TCPLIST_HH */
