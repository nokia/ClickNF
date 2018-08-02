/*
 * tcpflowlookup.{cc,hh} -- TCP flow lookup
 * Rafael Laufer, Massimo Gallo, Marco Trinelli
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
#include <click/error.hh>
#include <click/router.hh>
#include <rte_mbuf.h>
#include "tcpflowlookup.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "../userlevel/dpdk.hh"

CLICK_DECLS

TCPFlowLookup::TCPFlowLookup()
{
}

Packet *
TCPFlowLookup::smaction(Packet *p)
{
//	const click_ip *ip = p->ip_header();
//	const click_tcp *th = p->tcp_header();
    TCPState *s;
    struct rte_mbuf *mbuf;
    // Get flow tuple with our address as the source
    IPFlowID flow(p, true);

    if (DPDK::rss_hash_enabled) {
    // If the hash is already set in the NIC, get and set it on the flow
    // This avoids recomputing it (see ../include/lib/ipflowid.hh)
       mbuf = p->mbuf();
       if (mbuf->hash.rss)
           flow.set_hashcode(mbuf->hash.rss);
    }
	// If SYN packet, look for a listening socket to save a lookup. 
	// WARNING This will cause an error if a SYN packet is received 
	// for an ongoing connection
//  	if (TCP_SYN(th) && !TCP_ACK(th)){
//  		flow.set_daddr(IPAddress());
//  		flow.set_dport(0);
//  		s = TCPInfo::flow_lookup(flow);
//  	}
//  	else
//  		s = TCPInfo::flow_lookup(flow);

	// Get flow state
	s = TCPInfo::flow_lookup(flow);

	//If not found, try only our address/port and look for a server listening
	if (!s) {
		flow.set_daddr(IPAddress());
		flow.set_dport(0);
		if (DPDK::rss_hash_enabled)
		    flow.set_hashcode(0); // to let IPFlowID compute it
		s = TCPInfo::flow_lookup(flow);
	}

	if (s) {
		for (uint32_t i = 0; i < sizeof(TCPState); i += CLICK_CACHE_LINE_SIZE)
			prefetch0((char *)s + i);
	}
	
	// Set packet annotation
	SET_TCP_STATE_ANNO(p, (uint64_t)s);

	return p;
}

void
TCPFlowLookup::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPFlowLookup::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPFlowLookup)
