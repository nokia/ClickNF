/*
 * tcpenqueue4rtx.{cc,hh} -- enqueue packet for retransmission
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

#ifndef CLICK_TCPENQUEUE4RTC_HH
#define CLICK_TCPENQUEUE4RTC_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPEnqueue4RTX

=s tcp

enqueues TCP packets for retransmission

=d

This element expects a packet already encapsulated with both a TCP and an IP
header. If the packet has nothing in its sequence number space (i.e., no data
and no SYN/FIN flags) or if the packet is not the next one in sequence in the
RTX queue, it is not saved for retransmission. Otherwise, the packet is cloned
and the clone is inserted into the back of the RTX queue.

Keyword arguments are:

=over 8

=item VERBOSE

Boolean. If true, writes a message when packet is enqueued. Default is false.

=back

The TCPTimer element keeps a timer for retransmissions. Once this timer expires,
the head-of-line packet is retransmistted.

=e

Enqueues TCP segments for retransmission. 

    ... -> TCPAckEncap
        -> TCPIPEncap
        -> TCPEnqueue4RTX
        -> ...

=a TCPTimer */

class TCPEnqueue4RTX final : public Element { public:

	TCPEnqueue4RTX() CLICK_COLD;

	const char *class_name() const { return "TCPEnqueue4RTX"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	Packet *smaction(Packet *);
	void push(int, Packet *) final;
	Packet *pull(int);

  private:

	bool _verbose;

};

CLICK_ENDDECLS
#endif

