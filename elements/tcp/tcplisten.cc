/*
 * tcplisten.{cc,hh} -- handles TCP state LISTEN
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcplisten.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "tcptimers.hh"
CLICK_DECLS

TCPListen::TCPListen()
{
}

Packet *
TCPListen::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(ip && th);

	// RFC 793:
	// "First check for an RST. An incoming RST should be ignored."
	//
	// (...)
	//
	// "Do not process the FIN if the state is CLOSED, LISTEN or SYN-SENT
	//  since the SEG.SEQ cannot be validated; drop the segment and
	//  return."
	if (unlikely(TCP_RST(th) || TCP_FIN(th))) {
//		s->lock.release();
		p->kill();
		return NULL;
	}

	// "Second, check for an ACK. Any acknowledgment is bad if it arrives
	//  on a connection still in the LISTEN state.  An acceptable reset 
	//  segment should be formed for any arriving ACK-bearing segment. The
	//  RST should be formatted as follows:
	//
	//    <SEQ=SEG.ACK><CTL=RST>
	//
	//  Return."
	if (unlikely(th->th_flags & TH_ACK)) {
//		s->lock.release();
		SET_TCP_STATE_ANNO(p, 0);
		checked_output_push(1, p);
		return NULL;
	}

	// "Third, check for a SYN. If the SYN bit is set, (ignore security) 
	//  set RCV.NXT to SEG.SEQ+1, IRS is set to SEG.SEQ and any other
	//  control or text should be queued for processing later.  ISS
	//  should be selected and a SYN segment sent of the form:
	//
	//   <SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
	//
	//  SND.NXT is set to ISS+1 and SND.UNA to ISS.  The connection
	//  state should be changed to SYN-RECEIVED.  Note that any other
	//  incoming control or data (combined with SYN) will be processed
	//  in the SYN-RECEIVED state, but processing of SYN and ACK should
	//  not be repeated.  If the listen was not fully specified (i.e.,
	//  the foreign socket was not fully specified), then the
	//  unspecified fields should be filled in now.
	if (likely(TCP_SYN(th))) {
		// Ignore SYN packets with data
		if (unlikely(TCP_LEN(ip, th) > 0)) {
//			s->lock.release();
			p->kill();
			return NULL;
		}

		// Check if the accept queue is full
//		if (unlikely(s->acq.size() == uint32_t(s->backlog))) {
		if (unlikely(s->acq_size == s->backlog)) {
//			s->lock.release();
			p->kill();
			return NULL;
		}

		// If not, create a new state entry and populate it
		TCPState *t = TCPState::allocate();
		click_assert(t);

		// Get flow tuple with our address as the source
		IPFlowID flow(p, true);

		// Initialize TCB
		new(reinterpret_cast<void *>(t)) TCPState(flow);
		
		t->state      = TCP_SYN_RECV;
		t->flow       = flow;
		t->pid        = s->pid;
		t->sockfd     = -1;        // Filled later by accept()
		t->flags      = s->flags;
//		t->sk_flags   = s->sk_flags;
		t->task       = s->task;
//		t->wmem       = TCPInfo::wmem();
//		t->rmem       = TCPInfo::rmem();

//		t->rcv_isn    = TCP_SEQ(th);
//		t->rcv_nxt    = t->rcv_isn + 1;
		t->rcv_nxt    = TCP_SEQ(th) + 1;
		t->rcv_wnd    = TCPInfo::rmem();

		t->snd_isn    = click_random(0, 0xFFFFFFFF);
		t->snd_una    = t->snd_isn;
		t->snd_nxt    = t->snd_isn + 1;
		t->snd_wnd    = TCP_WIN(th);
		t->snd_wl1    = TCP_SEQ(th);
		t->snd_wl2    = TCP_ACK(th);

		t->is_passive = true;
		t->parent     = s;

		// Initialize timers
		unsigned c = click_current_cpu_id();
		t->rtx_timer.assign(TCPTimers::rtx_timer_hook, t);
		t->rtx_timer.initialize(TCPTimers::element(), c);

		t->tx_timer.assign(TCPTimers::tx_timer_hook, t);
		t->tx_timer.initialize(TCPTimers::element(), c);

#if HAVE_TCP_KEEPALIVE
		t->keepalive_timer.assign(TCPTimers::keepalive_timer_hook, t);
		t->keepalive_timer.initialize(TCPTimers::element(), c);
#endif

#if HAVE_TCP_DELAYED_ACK
		t->delayed_ack_timer.assign(TCPTimers::delayed_ack_timer_hook, t);
		t->delayed_ack_timer.initialize(TCPTimers::element(), c);
#endif

		// Unlock parent socket state and lock child state
//		s->lock.release();
//		t->lock.acquire();

		// Insert it into flow table
		TCPInfo::flow_insert(t);

		// Set packet annotation
		SET_TCP_STATE_ANNO(p, (uint64_t)t);

		// Process TCP options and send SYN-ACK
		return p;
	}

	// "Fourth, any other control or text-bearing segment (not containing 
	//  SYN) must have an ACK and thus would be discarded by the ACK
	//  processing. An incoming RST segment could not be valid, since
	//  it could not have been sent in response to anything sent by this
	//  incarnation of the connection.  So you are unlikely to get here,
	//  but if you do, drop the segment, and return."
//	s->lock.release();
	p->kill();
	return NULL;

}

void
TCPListen::push(int, Packet *p)
{
	p = smaction(p);
	if (likely(p))
		output(0).push(p);
}

Packet *
TCPListen::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPListen)
