/*
 * pi2info.hh -- pi2 double queueing information 
 * Myriana Rifai
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

#ifndef CLICK_PI2INFO_HH
#define CLICK_PI2INFO_HH
#include <click/element.hh>
#include <click/master.hh>
CLICK_DECLS

class PI2Info final : public Element { public:

	PI2Info() CLICK_COLD;

	const char *class_name() const	{ return "PI2Info"; }
	int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }

	// Pi2 Queues info API
	static inline bool verbose();
	static inline uint32_t get_cqtime();
    static inline void set_cqtime(uint32_t);
	static inline uint32_t get_lqtime();
    static inline void set_lqtime(uint32_t);
	
  private:

	static bool _verbose;
	static uint32_t _cqtime;
	static uint32_t _lqtime;

};

inline bool
PI2Info::verbose()
{
	return _verbose;
}

inline uint32_t
PI2Info::get_cqtime()
{
	return _cqtime;
}

inline void
PI2Info::set_cqtime(uint32_t cqtime)
{
	_cqtime = cqtime;
}

inline uint32_t
PI2Info::get_lqtime()
{
	return _lqtime;
}

inline void
PI2Info::set_lqtime(uint32_t lqtime)
{
	_lqtime = lqtime;
}

CLICK_ENDDECLS
#endif
