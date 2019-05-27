/*
 * tcptimer.{cc,hh} -- TCP timer implementation
 * Rafael Laufer, Massimo Gallo, Myriana Rifai
 *
 * Copyright (c) 2019 Nokia Bell Labs
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
#include <click/master.hh>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/routerthread.hh>
#include "tcptimer.hh"
#include "tcptimerset.hh"
CLICK_DECLS

TCPTimer::TCPTimer()
	: _bucket(-1), _callback(do_nothing_hook), _thunk(0), _owner(0), _thread(0)
{
}

TCPTimer::TCPTimer(TCPTimerCallback f, void *user_data)
	: _bucket(-1), _callback(f), _thunk(user_data), _owner(0), _thread(0)
{
}

void
TCPTimer::schedule_after(const Timestamp &delta)
{
	_thread->tcp_timer_set().schedule_after(this, delta);
}

void
TCPTimer::schedule_at_steady(const Timestamp &when_steady)
{
	_thread->tcp_timer_set().schedule_at_steady(this, when_steady);
}

void
TCPTimer::unschedule()
{
	if (scheduled())
		_thread->tcp_timer_set().unschedule(this);
}

void
TCPTimer::initialize(Element *owner, unsigned int thread_id, bool quiet)
{
	assert(!initialized() || _owner->router() == owner->router());
	_owner = owner;

	if (unlikely(_callback == do_nothing_hook && !_thunk) && !quiet)
		click_chatter("TCPTimer %p{element} [%p] does nothing", _owner, this);

	_thread = owner->master()->thread(thread_id);
}

void
TCPTimer::initialize(Element *owner, bool quiet)
{
	initialize(owner, owner->router()->home_thread_id(owner), quiet);
}

inline void 
TCPTimer::initialize(Router *router)
{
	initialize(router->root_element());
}

inline int 
TCPTimer::home_thread_id() const
{
	if (_thread)
		return _thread->thread_id();
	else
		return ThreadSched::THREAD_UNKNOWN;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPTimer)
ELEMENT_REQUIRES(TCPTimerSet)
