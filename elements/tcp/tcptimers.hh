/*
 * tcptimers.{cc,hh} -- TCP timers
 * Rafael Laufer, Myriana Rifai
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

#ifndef CLICK_TCPTIMERS_HH
#define CLICK_TCPTIMERS_HH
#include <click/element.hh>
#include <click/list.hh>
#include <click/timestamp.hh>
#include <click/timer.hh>
#include "tcpstate.hh"
#include "tcptimer.hh"
CLICK_DECLS

#define TCP_TIMERS_OUT_RTX 0  // Retransmission
#define TCP_TIMERS_OUT_KAL 1  // Keep-alive
#define TCP_TIMERS_OUT_ACK 2  // Delayed ACK
#define TCP_TIMERS_OUT_PACING  3

class TCPTimers final : public Element { public:

	TCPTimers() CLICK_COLD;

	const char *class_name() const { return "TCPTimers"; }
	const char *port_count() const { return "0/4"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

	static inline TCPTimers *element() { return _t; }

  private:

	static void rtx_timer_hook(TCPTimer *, void *);
#if BBR_ENABLED
	static void tx_timer_hook(TCPTimer *, void *);
#endif
	static void tw_timer_hook(TCPTimer *, void *);
#if HAVE_TCP_DELAYED_ACK
	static void delayed_ack_timer_hook(TCPTimer *, void *);
#endif
#if HAVE_TCP_KEEPALIVE
	static void keepalive_timer_hook(TCPTimer *, void *);
#endif

	static TCPTimers *_t;

	friend class TCPSocket;
	friend class TCPListen;
	friend class TCPProcessAck;
	friend class DCTCPProcessAck;
	friend class TCPProcessFin;
	friend class TCPProcessPkt;
};

CLICK_ENDDECLS
#endif
