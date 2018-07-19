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

#ifndef CLICK_TCPFLOWTABLE_HH
#define CLICK_TCPFLOWTABLE_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/ipflowid.hh>
#include <click/hashcontainer.hh>
#include "tcpstate.hh"
CLICK_DECLS

class TCPFlowTable final { public:

	TCPFlowTable() CLICK_COLD;
	TCPFlowTable(const TCPFlowTable& a);
	
	const char *class_name() const { return "TCPFlowTable"; }

	int configure(unsigned int) ;

	inline TCPState *lookup(const IPFlowID &flow);
	inline int insert(TCPState *s);
	inline int remove(TCPState *s);
	inline int remove(const IPFlowID &flow);

	typedef HashContainer<TCPState> FlowTable;

	static int h_flow(int, String&, Element*, const Handler*, ErrorHandler*);

  private:

	FlowTable _flowTable;

} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);

inline TCPState *
TCPFlowTable::lookup(const IPFlowID &flow)
{
	return _flowTable.get(flow);
}

inline int
TCPFlowTable::insert(TCPState *s)
{
	FlowTable::iterator it = _flowTable.find(s->flow);

	// Make sure there is no state for the same flow
	if (it != _flowTable.end())
		return -1;

	// Use insert_at() if possible, as it avoids an extra lookup
	if (likely(it.can_insert()))
		_flowTable.insert_at(it, s);
	else
		_flowTable.set(s);

	// Check if table is balanced
	if (unlikely(_flowTable.unbalanced())) {
		static int chatter = 0;
		if (chatter++ < 5)
			click_chatter("%s: rebalancing TCP flow table", class_name());

		_flowTable.balance();
	}

	return 0;
}

inline int
TCPFlowTable::remove(const IPFlowID &flow)
{
	TCPState *s = _flowTable.erase(flow);
	return (s == NULL ? -1 : 0);
}

inline int
TCPFlowTable::remove(TCPState *s)
{
	return remove(s->flow);
//	return _flowTable.erase(s);
//	if (s->list.isolated())
//		return -1;

//	s->list.detach();
//	return 0;
}

CLICK_ENDDECLS
#endif
