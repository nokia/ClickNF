/*
 * tcpnewrenortx.{cc,hh} -- retransmission (RFCs 5681/6582)
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


#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/tcp.h>
#include <click/timestamp.hh>
#include <click/straccum.hh>
#include "tcpnewrenortx.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "util.hh"
CLICK_DECLS

TCPNewRenoRTX::TCPNewRenoRTX()
{
}

Packet *
TCPNewRenoRTX::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	click_assert(s);
	
	// When a TCP sender detects segment loss using the retransmission timer
	// and the given segment has not yet been resent by way of the
	// retransmission timer, the value of ssthresh MUST be set to no more
	// than the value given in equation (4):
	// 
	//	   ssthresh = max (FlightSize / 2, 2*SMSS)			(4)
	// 
	// where, as discussed above, FlightSize is the amount of outstanding
	// data in the network.
	// 
	// On the other hand, when a TCP sender detects segment loss using the
	// retransmission timer and the given segment has already been
	// retransmitted by way of the retransmission timer at least once, the
	// value of ssthresh is held constant.
	if (s->snd_rtx_count == 1) {
		uint32_t mss = s->snd_mss;
		s->snd_ssthresh = MAX((s->snd_nxt - s->snd_una) >> 1, mss << 1);
	}

	// Further, if the SYN or SYN/ACK is lost, the initial window used by a
	// sender after a correctly transmitted SYN MUST be one segment
	// consisting of at most SMSS bytes.

	// Furthermore, upon a timeout (as specified in [RFC2988]) cwnd MUST be
	// set to no more than the loss window, LW, which equals 1 full-sized
	// segment (regardless of the value of IW).  Therefore, after
	// retransmitting the dropped segment the TCP sender uses the slow start
	// algorithm to increase the window from 1 full-sized segment to the new
	// value of ssthresh, at which point congestion avoidance again takes
	// over.

	// Both aforementioned comments are implemented by setting CWND to MSS
	s->snd_cwnd = s->snd_mss;

	// Reset duplicate ACK detection variable
	s->snd_dupack = 0;

	if (TCPInfo::verbose())
		click_chatter("%s: rtx, %s", class_name(), s->unparse_cong().c_str());

	return p;
}

void
TCPNewRenoRTX::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPNewRenoRTX::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPNewRenoRTX)
