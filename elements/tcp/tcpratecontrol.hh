/*
 * tcpratecontrol.{cc,hh} -- control transmission rate
 * Massimo Gallo, Rafael Laufer
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

#ifndef CLICK_TCPRATECONTROL_HH
#define CLICK_TCPRATECONTROL_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPRateControl

=s tcp

sets the WND annotation on each packet to the available TX window

=d

Incoming packets are expected to have the TCP state annotation set. The WND
annotation is set to the available TX window and the packet is pushed out. This
information is later used by the TCPDataPiggyback element to determine how much
data the packet will carry. After sending the first packet, if the window
allows (i.e., larger than MSS) and the TX queue is not emtpy, more packets may
be transmitted. After sending all allowed packets, the user task of the
incoming packet is woken if the TX queue is either empty or half-emtpy.

=e

The TCPRateControl element is only useful if the TCPDataPiggyback element is placed downstream to read the WND annotation and inject data into the packet:

    ... -> TCPRateControl
        -> TCPDataPiggyback
        -> TCPAckOptionsEncap
        -> TCPAckEncap
        -> TCPIPEncap
        -> ...

=a TCPDataPiggyback */

class TCPRateControl final : public Element { public:

	TCPRateControl() CLICK_COLD;

	const char *class_name() const { return "TCPRateControl"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	void push(int, Packet *) final;

};

CLICK_ENDDECLS
#endif

