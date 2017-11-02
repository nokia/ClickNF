/*
 * blockingtask.{cc,hh} -- Tasks that cooperatively yield the CPU to each other
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
=c

BlockingTask

=s tcp

Tasks that cooperatively yield the CPU to each other.

=d

Blocking tasks are backward compatible with regular tasks, and require no modifications to the Click task scheduler. 
In essence, the scheduler periodically runs the fire function of each task to start their execution. We make this a 
virtual function in the task class and design blocking tasks to inherit from that class. For blocking tasks, fire 
saves the scheduler execution context and restores the task context. The task then runs until it calls yield, which 
saves the task execution context and restores the scheduler context in order to allow another task to run.

=e



=a*/

#ifndef CLICK_USERTASK_HH
#define CLICK_USERTASK_HH
#include <click/element.hh>
#include <click/task.hh>
#if CLICK_USERLEVEL
# ifdef HAVE_BOOST_CONTEXT
#  include <boost/version.hpp>
#  include <boost/context/all.hpp>
# else
#  ifdef __APPLE__
#   define _XOPEN_SOURCE
#  endif
#  include <ucontext.h>
# endif
#endif
CLICK_DECLS

#define STACK_SIZE 65536  // 64 KB

class BlockingTask;
extern __thread BlockingTask *current;

#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 105200
using namespace boost::context;
# else
using namespace boost::ctx;
# endif
#endif // HAVE_BOOST_CONTEXT

class BlockingTask : public Task { public:

	inline BlockingTask(Element *e);
	inline BlockingTask(TaskCallback f, void *user_data);
	inline ~BlockingTask();

    inline bool fire();
	inline void yield(bool work_done = false);
	inline void yield_timeout(Timestamp &, bool work_done = false);

	inline void initialize(Element *owner, bool schedule);
	inline void initialize(Router *router, bool schedule);

 private:

	static void timer_hook(Timer *, void *);
#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
	typedef execution_context<intptr_t> econtext_t;
	static inline econtext_t wrapper(econtext_t &&ctx, intptr_t data);
# else
	static inline void wrapper(intptr_t data);
# endif
#else
	static inline void wrapper(void *data);
#endif // HAVE_BOOST_CONTEXT

	Timer _timer;
	bool _user_work_done;
	char *_stack;
#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
	econtext_t _app_ctx;
	econtext_t _network_ctx;
# else
	fcontext_t _app_ctx;
	fcontext_t _network_ctx;
# endif
#else
	ucontext_t _app_ctx;
	ucontext_t _network_ctx;
#endif
};

inline
BlockingTask::BlockingTask(Element *e)
	: Task(e), _user_work_done(false)
{
	_stack = new char [STACK_SIZE];
#ifdef HAVE_BOOST_CONTEXT
	// We assume that the stack grows down (e.g., x86 architectures)
# if BOOST_VERSION >= 106100
	_app_ctx = econtext_t(wrapper);
# elif BOOST_VERSION >= 105200
	_app_ctx = make_fcontext(_stack + STACK_SIZE, STACK_SIZE, wrapper);
# else
	_app_ctx.fc_stack.base = _stack + STACK_SIZE;
	_app_ctx.fc_stack.limit = _stack;
	make_fcontext(&_app_ctx, wrapper);
# endif
#else
	getcontext(&_app_ctx);
	_app_ctx.uc_stack.ss_sp = _stack;
	_app_ctx.uc_stack.ss_size = STACK_SIZE;
	_app_ctx.uc_stack.ss_flags = 0;
	_app_ctx.uc_link = &_network_ctx;
	makecontext(&_app_ctx, (void (*)())wrapper, 1, this);
#endif
}

inline
BlockingTask::BlockingTask(TaskCallback f, void *user_data)
	: Task(f, user_data), _user_work_done(false)
{
	_stack = new char [STACK_SIZE];
#ifdef HAVE_BOOST_CONTEXT
	// We assume that the stack grows down (e.g., x86 architectures)
# if BOOST_VERSION >= 106100
	_app_ctx = econtext_t(wrapper);
# elif BOOST_VERSION >= 105200
	_app_ctx = make_fcontext(_stack + STACK_SIZE, STACK_SIZE, wrapper);
# else
	_app_ctx.fc_stack.base = _stack + STACK_SIZE;
	_app_ctx.fc_stack.limit = _stack;
	make_fcontext(&_app_ctx, wrapper);
# endif
#else
	getcontext(&_app_ctx);
	_app_ctx.uc_stack.ss_sp = _stack;
	_app_ctx.uc_stack.ss_size = STACK_SIZE;
	_app_ctx.uc_stack.ss_flags = 0;
	_app_ctx.uc_link = &_network_ctx;
	makecontext(&_app_ctx, (void (*)())wrapper, 1, this);
#endif
}

inline
BlockingTask::~BlockingTask()
{
	delete [] _stack;
}

inline void
BlockingTask::initialize(Element *owner, bool schedule)
{
	_timer.assign(timer_hook, this);
	_timer.initialize(owner->router(), click_current_cpu_id());

	Task::initialize(owner, schedule);
}

inline void
BlockingTask::initialize(Router *router, bool schedule)
{
	_timer.assign(timer_hook, this);
	_timer.initialize(router, click_current_cpu_id());

	Task::initialize(router, schedule);
}

inline void
BlockingTask::yield(bool work_done)
{
	_user_work_done = work_done;
#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
	_network_ctx = std::move(std::get<0>(_network_ctx(0)));
#elif BOOST_VERSION >= 105600
	jump_fcontext(&_app_ctx, _network_ctx, 0);
# else
	jump_fcontext(&_app_ctx, &_network_ctx, 0);
# endif
#else
	swapcontext(&_app_ctx, &_network_ctx);
#endif
}

inline void
BlockingTask::yield_timeout(Timestamp &t, bool work_done)
{
	click_jiffies_t now = click_jiffies();

	_timer.schedule_after(t);
	yield(work_done);
	_timer.unschedule();

	t -= Timestamp::make_jiffies(click_jiffies() - now);
	if (t < 0)
		t = Timestamp(0);
}

#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
inline BlockingTask::econtext_t
BlockingTask::wrapper(econtext_t &&ctx, intptr_t data)
# else
inline void
BlockingTask::wrapper(intptr_t data)
# endif
#else
inline void
BlockingTask::wrapper(void *data)
#endif // HAVE_BOOST_CONTEXT
{
	BlockingTask *u = (BlockingTask *)data;
#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
	u->_network_ctx = std::move(ctx);
# endif
#endif

	// Run task
	if (u->_hook)
		u->_user_work_done = u->_hook(dynamic_cast<Task *>(u), u->_thunk);
	else
		u->_user_work_done = ((Element*)u->_thunk)->run_task(u);

#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
	return std::move(u->_network_ctx);
# elif BOOST_VERSION >= 105600
	jump_fcontext(&u->_app_ctx, u->_network_ctx, 0);
# else
	jump_fcontext(&u->_app_ctx, &u->_network_ctx, 0);
# endif
#endif // HAVE_BOOST_CONTEXT
}

inline bool
BlockingTask::fire()
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles(),
	start_child_cycles = this->_owner->_child_cycles;
#endif
#if HAVE_MULTITHREAD
	this->_cycle_runs++;
#endif
	current = this;
#ifdef HAVE_BOOST_CONTEXT
# if BOOST_VERSION >= 106100
	_app_ctx = std::move(std::get<0>(_app_ctx((intptr_t)this)));
# elif BOOST_VERSION >= 105600
	jump_fcontext(&_network_ctx, _app_ctx, (intptr_t)this);
# else
	jump_fcontext(&_network_ctx, &_app_ctx, (intptr_t)this);
# endif
#else
	if (swapcontext(&_network_ctx, &_app_ctx) == -1) {
		perror("BlockingTask::fire()");
		abort();
	}
#endif
	current = NULL;
	bool work_done = _user_work_done;
#if HAVE_ADAPTIVE_SCHEDULER
	++this->_runs;
	this->_work_done += work_done;
#endif
#if CLICK_STATS >= 2
	click_cycles_t all_delta = click_get_cycles() - start_cycles,
	own_delta = all_delta - (this->_owner->_child_cycles - start_child_cycles);
	this->_owner->_task_calls += 1;
	this->_owner->_task_own_cycles += own_delta;
#endif
	return work_done;
}

CLICK_ENDDECLS
#endif

