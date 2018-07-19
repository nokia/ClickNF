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

#ifndef CLICK_TCPPORTTABLE_HH
#define CLICK_TCPPORTTABLE_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/ipaddress.hh>
#include <click/ipflowid.hh>
#include <click/hashtable.hh>
#include "tcpstate.hh"
CLICK_DECLS

class TCPPortTable final { public:

	TCPPortTable();
	TCPPortTable(const TCPPortTable& a);
	
	const char *class_name() const	{ return "TCPPortTable"; }

	int configure(Vector<IPAddress>);

	typedef Vector<TCPState *> PortVector;
	typedef HashTable<IPAddress, PortVector> PortTable;

	inline void add(const IPAddress &addr);
	inline bool get(const IPAddress &addr, uint16_t port, TCPState *s);
	inline void put(const IPAddress &addr, uint16_t port);
	inline bool lookup(const IPAddress &addr, uint16_t port);

	static int h_port(int, String&, Element*, const Handler*, ErrorHandler*);

  private:

	PortTable _portTable;

} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);

inline bool
TCPPortTable::get(const IPAddress &addr, uint16_t port, TCPState *s)
{
	PortTable::iterator it = _portTable.find(addr);
	if (unlikely(it == _portTable.end()))
		return false;

	PortVector &v = it.value();
	if (!v[port]) {
		v[port] = s;
		return true;
	}

	return false;
}


inline void
TCPPortTable::add(const IPAddress &addr)
{
	_portTable.find_insert(addr, PortVector(65536, NULL));
}


inline void
TCPPortTable::put(const IPAddress &addr, uint16_t port)
{
	PortTable::iterator it = _portTable.find(addr);
	if (unlikely(it == _portTable.end()))
		return;

	PortVector &v = it.value();
	v[port] = NULL;
}

//TODO could have a port_get optimized after port_lookup is called
inline bool 
TCPPortTable::lookup(const IPAddress &addr, uint16_t port)
{
	PortTable::iterator it = _portTable.find(addr);
	if (unlikely(it == _portTable.end()))
		return false;

	PortVector &v = it.value();
	if (!v[port])
		return true;

	return false;
}

CLICK_ENDDECLS
#endif
