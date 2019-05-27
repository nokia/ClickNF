/*
 * tcptimer.{cc,hh} -- TCP timer implementation
 * Rafael Laufer, Massimo Gallo, Myriana Rifai
 *
 * Copyright (c) 20179 Nokia Bell Labs
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

#ifndef CLICK_TCPTIMER_HH
#define CLICK_TCPTIMER_HH
#include <click/element.hh>
#include <click/list.hh>
#include <click/timestamp.hh>
CLICK_DECLS

class TCPTimer;
class RouterThread;

typedef void (*TCPTimerCallback)(TCPTimer *timer, void *user_data);

class TCPTimer { public:

   /** @brief Construct a timer that does nothing when fired.
	 *
	 * This constructor is most useful for a timer that will be assigned a
	 * callback later, using TCPTimer::assign(). TCPTimer::initialize() will
	 * report a warning if called on a timer created by this constructor. */
	TCPTimer();

	/** @brief Construct a timer according to type.
	 * @param t TCP timer type
	 * @param s TCP state */
	TCPTimer(TCPTimerCallback f, void *data);

	/** @brief Destroy a timer, unscheduling it first if necessary. */
	inline ~TCPTimer() {
		unschedule();
	}

	/** @brief Change the timer to do nothing when fired. */
	inline void assign() {
		_callback = do_nothing_hook;
		_thunk = NULL;
	}

	/** @brief Change the timer to a different type. 
	 * @param f callback function
	 * @param user_data argument for callback function */
	inline void assign(TCPTimerCallback f, void *user_data) {
		_callback = f;
		_thunk = user_data;
	}

	/** @brief Return true iff the timer has been initialized. */
	inline bool initialized() const {
		return _owner != NULL;
	}

	/** @brief Initialize the timer and assigns it to a specific thread.
	 * @param owner the owner element
	 * @param thread_id the thread id where the timer should run 
	 * @param quiet do not produce default-constructor warning if true
	 *
	 * Before a timer can be used, it must be attached to a containing router.
	 * When that router is destroyed, the timer is automatically
	 * unscheduled.  It is safe to initialize the timer multiple times
	 * on the same router.
	 *
	 * If Click is compiled with statistics support, time spent in this
	 * Timer will be charged to the @a owner element.
	 *
	 * Initializing a TCPTimer constructed by the default constructor, 
	 * TCPTimer(), will produce a warning. */
	void initialize(Element *owner, unsigned int thread_id, bool quiet = false);
	
	/** @brief Initialize the timer.
	 * @param owner the owner element
	 * @param quiet do not produce default-constructor warning if true
	 *
	 * This function is shorthand for @link
	 * Timer::initialize(Element*,unsigned int,bool) Timer::initialize@endlink(
	 * owner, owner->router()->home_thread_id(owner)).
	 * However, it is better to explicitly associate timers with real 
	 * threads. */
	void initialize(Element *owner, bool quiet = false);

	/** @brief Initialize the timer.
	 * @param router the owner router
	 * @param thread_id the thread id where the timer should run
	 * 
	 * This function is shorthand for @link
	 * Timer::initialize(Element*,unsigned int,bool) Timer::initialize@endlink(
	 * @a router ->@link Router::root_element root_element@endlink(), 
	 * thread_id). However, it is better to explicitly associate timers with 
	 * a real thread and a real element. */
	void initialize(Router *router, unsigned int thread_id);

	/** @brief Initialize the timer.
	 * @param router the owner router
	 *
	 * This function is shorthand for @link
	 * TCPTimer::initialize(Element *, bool) TCPTimer::initialize@endlink(@a
	 * router ->@link Router::root_element root_element@endlink()).
	 * However, it is better to explicitly associate timers with real
	 * elements. */
	inline void initialize(Router *router);

	/** @brief Schedule the timer to fire @a delta time in the future.
	 * @param delta interval until expiration time
	 *
	 * The schedule_after methods schedule the timer relative to the current
	 * time.  When called from a timer's callback function, this will usually
	 * be slightly after the timer's nominal expiration time.  To schedule a
	 * timer at a strict interval, compensating for small amounts of drift,
	 * use the reschedule_after methods. */
	void schedule_after(const Timestamp &delta);

	/** @brief Schedule the timer to fire after @a delta_sec seconds.
	 * @param delta_sec interval until expiration time, in seconds
	 *
	 * @sa schedule_after, reschedule_after_sec */
	inline void schedule_after_sec(uint32_t delta_sec) {
		schedule_after(Timestamp(delta_sec, 0));
	}

	/** @brief Schedule the timer to fire after @a delta_msec milliseconds.
	 * @param delta_msec interval until expiration time, in milliseconds
	 *
	 * @sa schedule_after, reschedule_after_msec */
	inline void schedule_after_msec(uint32_t delta_msec) {
		schedule_after(Timestamp::make_msec(delta_msec));
	}

	/** @brief Schedule the timer to fire @a delta_usec microseconds.
	 * @param delta_usec interval until expiration time, in microseconds
	 *
	 * @sa schedule_after_usec, schedule_after */
	inline void schedule_after_usec(uint32_t delta_usec) {
		schedule_after(Timestamp::make_usec(delta_usec));
	}

	/** @brief Schedule the timer to fire at @a when_steady.
	 * @param when_steady expiration time according to the steady clock
	 *
	 * If @a when_steady is more than 2 seconds behind the current time, then
	 * the expiration time is silently updated to the current time.
	 *
	 * @sa schedule_at() */
	void schedule_at_steady(const Timestamp &when_steady);

    /** @brief Shedule the timer to fire immediately.
     *
     * Equivalent to schedule_at(Timestamp::recent()). */
    inline void schedule_now() {
		schedule_after(Timestamp(0, 0));
    }

   /** @brief Schedule the timer to fire @a delta time after its previous
	 * expiry.
	 * @param delta interval until expiration time
	 *
	 * If the expiration time is too far in the past, then the new expiration
	 * time will be silently updated to the current system time.
	 *
	 * @sa schedule_after */
	inline void reschedule_after(const Timestamp &delta) {
		schedule_at_steady(_expiry + delta);
	}

	/** @brief Schedule the timer to fire @a delta_sec seconds after its
	 * previous expiry.
	 * @param delta_sec interval until expiration time, in seconds
	 *
	 * @sa schedule_after_sec, reschedule_after */
	inline void reschedule_after_sec(uint32_t delta_sec) {
		reschedule_after(Timestamp::make_sec(delta_sec));
	}

	/** @brief Schedule the timer to fire @a delta_msec milliseconds after its
	 * previous expiry.
	 * @param delta_msec interval until expiration time, in milliseconds
	 *
	 * @sa schedule_after_msec, reschedule_after */
	inline void reschedule_after_msec(uint32_t delta_msec) {
		reschedule_after(Timestamp::make_msec(delta_msec));
	}

	/** @brief Schedule the timer to fire @a delta_usec microseconds after its
	 * previous expiry.
	 * @param delta_usec interval until expiration time, in microseconds
	 *
	 * @sa schedule_after_usec, reschedule_after */
	inline void reschedule_after_usec(uint32_t delta_usec) {
		reschedule_after(Timestamp::make_usec(delta_usec));
	}

	/** @brief Unschedule the timer.
	 *
	 * The timer's expiration time is not modified. */
	void unschedule();

	/** @brief Unschedule the timer and reset its expiration time. */
	inline void clear() {
		unschedule();
		_expiry = Timestamp();
	}

	/** @brief Return true iff the timer is currently scheduled. */
	inline bool scheduled() const {
		return _bucket >= 0;
	}

	/** @brief Return the Timer's steady-clock expiration time.
	 *
	 * This is the absolute time, according to the steady clock, at which the
	 * timer is next scheduled to fire.  If the timer is not currently
	 * scheduled, then expiry_steady() returns the last assigned expiration
	 * time.
	 *
	 * @sa expiry() */
	inline const Timestamp &expiry_steady() const {
		return _expiry;
	}

	/** @brief Return the timer's system-clock expiration time.
	 *
	 * Timer expirations are measured using the system's steady clock, which
	 * increases monotonically.  (See Timestamp::now_steady().)  The expiry()
	 * function, however, returns the timer's expiration time according to the
	 * system clock.  This is a calculated value: if the system clock changes
	 * -- because the user changes the current system time, for example --
	 * then the timer's expiry() will also change.  (The timer's
	 * expiry_steady() value will not change, however.)
	 *
	 * @sa expiry_steady() */
	inline Timestamp expiry() const {
		if (_expiry)
			return _expiry + Timestamp::recent() - Timestamp::recent_steady();
		else
			return _expiry;
	}

	/** @brief Return the timer's associated Router. */
	inline Router *router() const {
		return (_owner ? _owner->router() : NULL);
	}

	/** @brief Return the timer's owning element. */
	inline Element *element() const {
		return _owner;
	}

	/** @brief Return the Timer's associated RouterThread. */
	inline RouterThread *thread() const {
		return _thread;
	}

	/** @brief Return the Timer's associated home thread ID. */
	inline int home_thread_id() const;

  private:

	List_member<TCPTimer> _link;
	int _bucket;
	Timestamp _expiry;
	TCPTimerCallback _callback;
	void *_thunk;
	Element *_owner;
	RouterThread *_thread;

	TCPTimer &operator=(const TCPTimer &t);

	static inline void do_nothing_hook(TCPTimer *, void *);

	friend class TCPTimers;
	friend class TCPTimerSet;
};

inline void
TCPTimer::do_nothing_hook(TCPTimer *, void *)
{
}

CLICK_ENDDECLS
#endif
