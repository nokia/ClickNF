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

#include <click/config.h>
#include <click/task.hh>
#include <click/routerthread.hh>
#include <click/master.hh>
#include <clicknet/tcp.hh>
#include "tcptimerset.hh"
#include "util.hh"
CLICK_DECLS

static double tick_msecval = 0.0;
static uint32_t mask = 0;

TCPTimerSet::TCPTimerSet() : _idx(0), _size(0)
{
#if CLICK_NS
	_max_timer_stride = 1;
#else
	_max_timer_stride = 32;
#endif
	_timer_stride = _max_timer_stride;
	_timer_count = 0;

	_tick = Timestamp::make_msec(1);
	tick_msecval = 1/_tick.msecval();

	uint32_t m = 0;
	m = MAX(m, TCP_RTO_MAX);
#if HAVE_TCP_DELAYED_ACK
	m = MAX(m, TCP_DELAYED_ACK);
#endif
#if HAVE_TCP_KEEPALIVE
	m = MAX(m, TCP_KEEPALIVE);
#endif
	m = MAX(m, TCP_MSL << 1);

	// Give 500ms of possible asynchrony between real time and wheel time
	_nbuckets = m/_tick.msecval() + 500 + 1;

	// Make the number of buckets a power of 2
	uint32_t n = 1;
	while (n < _nbuckets)
		n <<= 1;

	_nbuckets = n;
	mask = n - 1;

	// Allocate buckets
	_bucket = new TCPTimerList [_nbuckets];
	click_assert(_bucket);

	_now = Timestamp::now_steady().msec_ceil();
#if CLICK_LINUXMODULE
    _task = 0;
#elif HAVE_MULTITHREAD
    _processor = click_invalid_processor();
#endif
}

void
TCPTimerSet::run_timers(RouterThread *thread, Master *master)
{
#if CLICK_LINUXMODULE
	if (_task == 0)
		_task = current;
	assert(_task == current);
#elif HAVE_MULTITHREAD
	if (_processor == click_invalid_processor())
		_processor = click_current_processor();
	assert(_processor == click_current_processor());
#endif

	// Check if timers are supposed to run
	if (_size == 0 || master->paused() || thread->stop_flag())
		return;

	// Set thread state
	thread->set_thread_state(RouterThread::S_RUNTIMER);

	// Get now
	Timestamp now = Timestamp::now_steady();

	if (_now <= now) {
		// Potentially adjust timer stride
		if (_now + _tick/2 <= now) {
			_timer_count = 0;
			if (_timer_stride > 1)
				_timer_stride = (_timer_stride * 4) / 5;
		}
		else if (++_timer_count >= 12) {
			_timer_count = 0;
			if (++_timer_stride >= _max_timer_stride)
				_timer_stride = _max_timer_stride;
		}

		// Fire expired timers
		do {
			// Get timer bucket
			TCPTimerList &l = _bucket[_idx];

			// Go over each timer and fire it
			TCPTimerList::iterator it = l.begin();
			while (it != l.end()) {
				TCPTimer *t = it.get();
				it++;
				unschedule(t);

				click_assert(t->_expiry == _now);
				run_one_timer(t);
			}

			// Update index
			//_idx = (_idx + 1) % _nbuckets;
			_idx = (_idx + 1) & mask;

			// Update timing wheel timestamp
			_now += _tick;

		} while (_now <= now);
	}
}

void
TCPTimerSet::schedule_at_steady(TCPTimer *t, Timestamp when_steady)
{
	click_assert(t);

	// If timer is scheduled, remove it from timing wheel
	if (t->scheduled())
		unschedule(t);

	// If no pending timers, reset timing wheel
	if (_size == 0) {
		_idx = 0;
		_now = Timestamp::now_steady().msec_ceil();
		t->_thread->wake();
	}

	// Round expiration time to millisecond granularity
	when_steady = when_steady.msec_ceil();

	// Compute delta
	Timestamp delta = (when_steady < _now ? _tick : when_steady - _now);

	// Compute ticks
	Timestamp::value_type ticks = delta.msecval() * tick_msecval;
	click_assert(ticks < _nbuckets);

	// Compute bucket
	// NOTE & can be used if  (_idx + ticks) is a power of 2
	//int b = (_idx + ticks) % _nbuckets;
	int b = (_idx + ticks) & mask;

	// Save bucket
	t->_bucket = b;

	// Save expiry
	t->_expiry = _now + delta;

	// Update timing wheel
	_bucket[b].push_back(t);
	_size++;
}

void
TCPTimerSet::unschedule(TCPTimer *t)
{
	click_assert(t);

	// If not scheduled, return
	if (!t->scheduled())
		return;

	// Get bucket
	int b = t->_bucket;

	// Update timing wheel
	_bucket[b].erase(t);
	_size--;

	// Reset bucket
	t->_bucket = -1;
}


void
TCPTimerSet::kill_router(Router *router)
{
	assert(_processor == click_current_processor());

	for (uint32_t i = 0; i < _nbuckets; i++) {
		// Get timer bucket
		TCPTimerList &l = _bucket[i];

		TCPTimerList::iterator it = l.begin();
		while (it != l.end()) {
			TCPTimer *t = it.get();
			it++;

			if (t->router() == router) {
				unschedule(t);
				t->_owner = NULL;
			}
		}
	}
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPTimerSet)
