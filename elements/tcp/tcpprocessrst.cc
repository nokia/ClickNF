/*
 * tcpprocessrst.{cc,hh} -- Process TCP RST flag
 * Rafael Laufer, Massimo Gallo
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
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpprocessrst.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
CLICK_DECLS

TCPProcessRst::TCPProcessRst()
{
}

Packet *
TCPProcessRst::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
//	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);

	// RFC 793:
	// "second check the RST bit"
	if (likely(!TCP_RST(th)))
		return p;

	// Stop timers and flush queues
	s->stop_timers();
	s->flush_queues();

	switch (s->state) {
	case TCP_SYN_RECV:

		// "If this connection was initiated with a passive OPEN (i.e.,
		//  came from the LISTEN state), then return this connection to
		//  LISTEN state and return.  The user need not be informed.  If
		//  this connection was initiated with an active OPEN (i.e., came
		//  from SYN-SENT state) then the connection was refused, signal
		//  the user "connection refused".  In either case, all segments
		//  on the retransmission queue should be removed.  And in the
		//  active OPEN case, enter the CLOSED state and delete the TCB,
		//  and return."

		// If not a passive socket, notify error to user
		if (!s->is_passive)
			s->notify_error(ECONNRESET);
		else {
			// Get parent TCB
			TCPState *t = s->parent;

			// Remove it from the accept queue of the parent
			if (t) {
//				t->lock.acquire();
//				for (TCPState::AcceptQueue::iterator it = t->acq.begin(); it; it++)
//					if (it.get() == s) {
//						t->acq.erase(it);
//						break;
//					}
//				s->acq_link.detach();
				t->acq_erase(s);

//				t->lock.release();
			}
		}

		// Remove it from the flow table
		TCPInfo::flow_remove(s);

		// Unlock state
//		s->lock.release();

		// Wait for a grace period and deallocate TCB
//		synchronize_rcu();
		TCPState::deallocate(s);

		break;

	case TCP_ESTABLISHED:
	case TCP_CLOSE_WAIT:

		// "If the RST bit is set then, any outstanding RECEIVEs and SEND
		//  should receive "reset" responses.  All segment queues should be
		//  flushed.  Users should also receive an unsolicited general
		//  "connection reset" signal.  Enter the CLOSED state, delete the
		//  TCB, and return."

		// Remove it from the port table
		if (!s->is_passive) {
			uint16_t port = ntohs(s->flow.sport());
			TCPInfo::port_put(s->flow.saddr(), port);
		}

		// Remove it from the flow table
		TCPInfo::flow_remove(s);

		// Notify error to user
		s->notify_error(ECONNRESET);

		// Unlock state
//		s->lock.release();

		break;

	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
	case TCP_CLOSING:
	case TCP_LAST_ACK:
	case TCP_TIME_WAIT:

		// "If the RST bit is set then, enter the CLOSED state, delete the
		//  TCB, and return."

		// Remove it from the port table
		if (!s->is_passive) {
			uint16_t port = ntohs(s->flow.sport());
			TCPInfo::port_put(s->flow.saddr(), port);
		}

		// Remove it from the flow table
		TCPInfo::flow_remove(s);

		// Unlock state
//		s->lock.release();

		// Wait for a grace period and deallocate TCB
//		synchronize_rcu();
		TCPState::deallocate(s);

		break;

	default:
		assert(0);
	}

	s->state = TCP_CLOSED;

	p->kill();
	return NULL;
}

void
TCPProcessRst::push(int, Packet *p)
{
	p = smaction(p);
	if (likely(p))
		output(0).push(p);
}

Packet *
TCPProcessRst::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPProcessRst)
