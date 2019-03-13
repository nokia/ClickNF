/*
 * tcpeventqueue.{cc,hh} -- the TCP event queue class
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

#ifndef CLICK_TCPEVENTQUEUE_HH
#define CLICK_TCPEVENTQUEUE_HH
#include <click/string.hh>
#include "tcphashallocator.hh"
#include "tcplist.hh"
CLICK_DECLS

class TCPState;

class TCPEvent { public:

	TCPEvent(): state(NULL), event(0) { }
	TCPEvent(const TCPEvent &tev): state(tev.state), event(tev.event) { }
	TCPEvent(TCPState* s, uint16_t e): state(s), event(e) { }
	
	TCPState *state;
	uint16_t event;
	TCPList_member link;
}; 

class TCPEventQueue { public:

	TCPEventQueue() { }

// 	TCPEvent *allocate();
// 	TCPEvent *allocate(TCPState *s);
// 	void deallocate(TCPEvent *e);

	inline TCPEvent *front() {
		return _eventQueue.front();
	}
	
	inline TCPList<TCPEvent, &TCPEvent::link>::iterator begin() {
		return _eventQueue.begin();
	}

	inline TCPList<TCPEvent, &TCPEvent::link>::iterator end() {
		return _eventQueue.end();
	}
	
	inline void erase(TCPEvent* ev) {
		return _eventQueue.erase(ev);
	}
	
	inline void pop_front() {
		_eventQueue.pop_front();
	}
	
	inline void push_back(TCPEvent *e) {
		_eventQueue.push_back(e);
	}

	inline int size() const {
		return _eventQueue.size();
	}

	typedef TCPList<TCPEvent, &TCPEvent::link>::iterator iterator;
  private:

	TCPList<TCPEvent, &TCPEvent::link> _eventQueue;
};

CLICK_ENDDECLS
#endif
