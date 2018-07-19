/*
 * tcpflowtable.{hh,cc} -- TCP flow table
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
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include "tcpflowtable.hh"
CLICK_DECLS

TCPFlowTable::TCPFlowTable()
{
}

TCPFlowTable::TCPFlowTable(const TCPFlowTable& a)
{ 
	// Copy constructor for empty HashContainer only
	assert(a._flowTable.size() == 0);
}

int
TCPFlowTable::configure(unsigned int _buckets)
{
	size_t buckets = TCP_FLOW_BUCKETS;

	if (_buckets)
		buckets = _buckets;

	_flowTable.rehash(buckets);

    return 0;
}

int
TCPFlowTable::h_flow(int, String &s, Element *e, 
                                            const Handler *, ErrorHandler *errh)
{
	TCPFlowTable *t = (TCPFlowTable *)e->cast("TCPFlowTable");
	if (!t)
		return errh->error("not a TCPFlowTable element");

	StringAccum sa;
	sa << "Proto  Recv-Q  Send-Q  ";
	sa << "Local Address          Foreign Address         State";
	for (FlowTable::const_iterator it = t->_flowTable.begin(); it; it++) {
		const TCPState *s = it.get();

		sa << "tcp  ";
		sa << "  ";
		sa.snprintf(6, "%6u", s->rxq.packets() + s->rxb.packets());
		sa << "  ";
		sa.snprintf(6, "%6u", s->txq.packets());
		sa << "  ";
		sa << s->flow.saddr().unparse() << ':' << s->flow.sport();
		sa.append_fill(' ', 46 - sa.length());
		sa << s->flow.daddr().unparse() << ':' << s->flow.dport();
		sa.append_fill(' ', 69 - sa.length());
		sa << s->unparse() << '\n';

	}

	s = sa.take_string();

	return 0;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPFlowTable)
