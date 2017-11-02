/*
 * packetqueue.{cc,hh} -- a packet linked list
 * Rafael Laufer
 *
 * Copyright (c) 2016 Nokia Bell Labs
 *
 */


#ifndef CLICK_PACKETQUEUE_HH
#define CLICK_PACKETQUEUE_HH
#include <click/packet.hh>
#define CLICK_DEBUG_PACKETQUEUE 0
#if CLICK_DEBUG_PACKETQUEUE
# define pq_assert(x) assert(x)
#else
# define pq_assert(x) ((void)(0))
#endif
CLICK_DECLS

class PacketQueue { public:

	/** @brief Construct an empty PacketQueue. */
	inline PacketQueue() : _head(NULL), _tail(NULL), _size(0) {
	}

	/** @brief Destroy a PacketQueue, killing enqueued packets if necessary. */
	inline ~PacketQueue() {
		clear();
	}

	/** @brief Return the first packet in the queue. */
	inline Packet *front() const {
		return _head;
	}

	/** @brief Return the last packet in the queue. */
	inline Packet *back() const {
		return _tail;
	}

	/** @brief Return true iff size() == 0. */
	inline bool empty() const {
		return _size == 0;
	}

	/** @brief Return the number of packets in the queue. */
	inline size_t size() const {
		return _size;
	}

	/** @brief Insert a new packet at the end of the queue.
	 * @param p packet to be inserted into the queue */
	inline void push_back(Packet *p) {
		p->set_next(NULL);
		if (empty())
			_head = p;
		else
			_tail->set_next(p);

		_tail = p;
		_size++;
	}

	/** @brief Insert a new packet at the beginning of the queue.
	 * @param p packet to be inserted into the queue */
	inline void push_front(Packet *p) {
		p->set_next(_head);
		if (empty())
			_tail = p;
		_head = p;
		_size++;
	}

	/** @brief Remove the packet at the beginning of the queue. */
	inline void pop_front() {
		Packet *p = _head;
		if (p) {
			if (p == _tail)
				_tail = NULL;
			_head = p->next();
			_size--;
		}
	}

	/** @brief Destroy the queue, killing enqueued packets if necessary. */
	inline void clear() {
		while (_head) {
			Packet *p = _head;
			_head = p->next();
			p->kill();
			_size--;
		}
		pq_assert(_size == 0);
	}

  private:

	Packet *_head;
	Packet *_tail;
	size_t _size;

};


CLICK_ENDDECLS
#endif
