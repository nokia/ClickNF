/*
 * tcpsynsent.{cc,hh} -- handles TCP state SYN_SENT
 * Rafael Laufer
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

#ifndef CLICK_TCPSYNSENT_HH
#define CLICK_TCPSYNSENT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPSynSent

=s tcp

handles packets whose connection is in SYN_SENT state

=d

If the packet has the FIN flag set or the SYN flag reset, it is killed. If the
ACK flag is set but the ACK is not acceptable, the packet is sent to output 2
for a RST to be sent (or discarded if there is no output 2). If both the SYN and
the ACK flags are set and the ACK is acceptable, the connection enters the
ESTABLISHED state and the packet is sent to output 0 for further processing
(e.g., SYN options). If only the SYN flag is set, then a simultaneous open is
assumed to occur. The connection enters the SYN_RECV state, the packet is sent
to output 1 for further processing and finally sending the SYN-ACK packet.

=a TCPState, TCPStateDemux */

class TCPSynSent final : public Element { public:

	TCPSynSent() CLICK_COLD;

	const char *class_name() const { return "TCPSynSent"; }
	const char *port_count() const { return "1/2-3"; }
	const char *processing() const { return PROCESSING_A_AH; }

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

};

CLICK_ENDDECLS
#endif

