/*
 * tcpprocesssyn.{cc,hh} -- Process TCP SYN flag
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
#include "tcpprocesssyn.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "util.hh"
CLICK_DECLS

TCPProcessSyn::TCPProcessSyn()
{
}

Packet *
TCPProcessSyn::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
//	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && th);

	// RFC 793:
	// "third check security and precedence (ignored)"
	//
	// "fourth, check the SYN bit
	//      If the SYN is in the window it is an error, send a reset, any
	//      outstanding RECEIVEs and SEND should receive "reset" responses,
	//      all segment queues should be flushed, the user should also
	//      receive an unsolicited general "connection reset" signal, enter
	//      the CLOSED state, delete the TCB, and return.
	//
	//      If the SYN is not in the window this step would not be reached
	//      and an ack would have been sent in the first step (sequence
	//      number check)."
	//
	if (likely(!TCP_SYN(th)))
		return p;

	// Stop timers and flush queues
	s->stop_timers();
	s->flush_queues();

	// RFC 1122:
	// "(e)  Check SYN bit, p. 71:  "In SYN-RECEIVED state and if
	//       the connection was initiated with a passive OPEN, then
	//       return this connection to the LISTEN state and return.
	//       Otherwise...".
	switch (s->state) {
	case TCP_SYN_RECV:
		// If not a passive socket, notify error to user
		if (!s->is_passive)
			s->notify_error(ECONNRESET);
		else {
			// Get parent TCB
			TCPState *t = s->parent;
			
			// Remove it from the accept queue of the parent
			if (t) {
//				for (TCPState::AcceptQueue::iterator it = t->acq.begin(); it; it++)
//					if (it.get() == s)
//						t->acq.erase(it);
//				s->acq_link.detach();
				t->acq_erase(s);
			}

			// Remove it from the flow table
			TCPInfo::flow_remove(s);

			// Wait for a grace period and deallocate TCB
//			synchronize_rcu();
			TCPState::deallocate(s);
		}
		break;

	case TCP_ESTABLISHED:
	case TCP_CLOSE_WAIT:

		// Notify error to user
		s->notify_error(ECONNRESET);
		break;

	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
	case TCP_CLOSING:
	case TCP_LAST_ACK:
	case TCP_TIME_WAIT:

		// Remove it from the port table
		if (!s->is_passive) {
			uint16_t port = ntohs(s->flow.sport());
			TCPInfo::port_put(s->flow.saddr(), port);
		}

		// Remove it from the flow table
	    TCPInfo::flow_remove(s);

	    // Wait for a grace period and deallocate TCB
//	    synchronize_rcu();
	    TCPState::deallocate(s);

	    break;

	default:
		assert(0);
	}

	// Send the reset
	output(1).push(p);

	return NULL;
}

void
TCPProcessSyn::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPProcessSyn::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPProcessSyn)

