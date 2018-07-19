/*
 * tcptimerset.{cc,hh} -- timing wheel implementation for TCP timers
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

#ifndef CLICK_TCPTIMERSET_HH
#define CLICK_TCPTIMERSET_HH
#include <click/sync.hh>
#include <click/list.hh>
#include <click/glue.hh>
#include "tcptimer.hh"
CLICK_DECLS

class Router;
class RouterThread;
class TCPTimer;

class TCPTimerSet { public:

	TCPTimerSet();

	void kill_router(Router *router);
	void run_timers(RouterThread *thread, Master *master);

	inline unsigned max_timer_stride() const { return _max_timer_stride; }
	inline unsigned timer_stride() const     { return _timer_stride; }
 	inline void set_max_timer_stride(unsigned timer_stride) {
		_max_timer_stride = timer_stride;
		if (_timer_stride > _max_timer_stride)
			_timer_stride = _max_timer_stride;
	}

	typedef List<TCPTimer, &TCPTimer::_link> TCPTimerList;

  private:

	inline void run_one_timer(TCPTimer *);
    inline void schedule_after(TCPTimer *, Timestamp);
	void schedule_at_steady(TCPTimer *, Timestamp);
    void unschedule(TCPTimer *);


	TCPTimerList *_bucket;
	Timestamp _now;
	Timestamp _tick;
	uint32_t _nbuckets;
	uint32_t _idx;
	uint32_t _size;

	unsigned _timer_count;
	unsigned _timer_stride;
	unsigned _max_timer_stride;
	
#if CLICK_LINUXMODULE
	struct task_struct *_task;
#else
	click_processor_t _processor;
#endif

	friend class TCPTimer;

};

inline void
TCPTimerSet::run_one_timer(TCPTimer *t)
{
	// Run callback function 
#if CLICK_STATS >= 2
	Element *owner = t->_owner;
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t start_child_cycles = owner->_child_cycles;
#endif
	t->_callback(t, t->_thunk);
#if CLICK_STATS >= 2
	click_cycles_t all_delta = click_get_cycles() - start_cycles;
	click_cycles_t child_delta = owner->_child_cycles - start_child_cycles;
	click_cycles_t own_delta = all_delta - child_delta;
	owner->_timer_calls += 1;
	owner->_timer_own_cycles += own_delta;
#endif
}

inline void
TCPTimerSet::schedule_after(TCPTimer *t, Timestamp delta)
{
	assert(!delta.is_negative());
	schedule_at_steady(t, Timestamp::now_steady() + delta);
}

CLICK_ENDDECLS
#endif
