/*
 * tcpporttable.{hh,cc} -- TCP port table
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
#include "tcpporttable.hh"
CLICK_DECLS

TCPPortTable::TCPPortTable()
{
}

TCPPortTable::TCPPortTable(const TCPPortTable& a)
	: _portTable(a._portTable)
{ 
}

int
TCPPortTable::configure(Vector<IPAddress> addr)
{
	size_t buckets = TCP_FLOW_BUCKETS;

	if (addr.empty()){
		click_chatter("ADDRS must be given at least one IP address");
		exit(0);
	}
	if (buckets == 0){
		click_chatter("BUCKETS must be positive");
		exit(0);
	}
	
	_portTable.rehash(buckets);

	for (int i = 0; i < addr.size(); i++)
		_portTable.find_insert(addr[i], PortVector(65536, NULL));

    return 0;
}

int
TCPPortTable::h_port(int, String &s, Element *e, const Handler*, ErrorHandler* errh)
{
	TCPPortTable *t = (TCPPortTable *)e->cast("TCPPortTable");
	if (!t)
		return errh->error("not a TCPPortTable element");

	StringAccum sa;
	sa << "Proto  Local Address          Foreign Address        State";
	for (PortTable::const_iterator it = t->_portTable.begin(); it; it++) {
		const PortVector &v = it.value();
		for (int port = 0; port < 65536; port++) {
			const TCPState *s = v[port];

			if (s) {
				sa << "tcp    ";
				sa << s->flow.saddr().unparse() << ':' << s->flow.sport();
				sa.append_fill(' ', 30 - sa.length());
				sa << s->flow.daddr().unparse() << ':' << s->flow.dport();
				sa.append_fill(' ', 53 - sa.length());
				sa << s->unparse() << '\n';
			}
		}
	}

	s = sa.take_string();

	return 0;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPPortTable)
