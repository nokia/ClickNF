/*
 * tcpsetmssanno.{cc,hh} -- sets the TCP MSS annotation
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

#ifndef CLICK_TCPSETMSSANNO_HH
#define CLICK_TCPSETMSSANNO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPSetMssAnno

=s tcp

sets the packet MSS annotation

=d

The packet MSS annotation is set to the MSS in the TCP control block (TCB).
If the TCP state annotation is not set, then the MSS annotation is set to the
configuration parameter. This is useful when TCP segment offloading is active
in the NIC and the device needs to know the MSS in order to segment a large
packet.

=e
    InfiniteSource(LENGTH 2048)
        -> TCPSetMssAnno(1460)
        -> TCPEncap(1111, 2222, SEQNO 1, ACKNO 1, ACK true, WINDOW 65535)
        -> IPEncap(tcp, 1.1.1.1, 2.2.2.2)
        -> EtherEncap(0x0800, 01:01:01:01:01:01, 02:02:02:02:02:02)
        -> DPDK(00:01:02:03:04:05, TX_TCP_TSO 1, ...)

    InfiniteSource(LENGTH 2048)
        -> TCPSetMssAnno(1460)
        -> TCPEncap(1111, 2222, SEQNO 1, ACKNO 1, ACK true, WINDOW 65535)
        -> IPEncap(tcp, 1.1.1.1, 2.2.2.2)
        -> EtherEncap(0x0800, 01:01:01:01:01:01, 02:02:02:02:02:02)
        -> DPDK(00:01:02:03:04:05, TX_TCP_TSO 1, ...)

=a DPDK */

class TCPSetMssAnno final : public Element { public:

	TCPSetMssAnno() CLICK_COLD;

	const char *class_name() const  { return "TCPSetMssAnno"; }
	const char *port_count() const  { return PORTS_1_1; }
	const char *processing() const  { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

  private:

	uint16_t _mss;

};

CLICK_ENDDECLS
#endif
