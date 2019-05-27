/*
 * tcpnewrenosyn.{cc,hh} -- initialize congestion control (RFCs 5681/6582)
 * Massimo Gallo, Rafael Laufer, Myriana Rifai
 *
 * Copyright (c) 2019 Nokia Bell Labs
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
#include <click/straccum.hh>
#include "tcpnewrenosyn.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
CLICK_DECLS

TCPNewRenoSyn::TCPNewRenoSyn()
{
}

Packet *
TCPNewRenoSyn::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p); 
	click_assert(s && TCP_SYN(p));

	// RFC 5681:
	//
	// IW, the initial value of cwnd, MUST be set using the following
	// guidelines as an upper bound.
	// 
	// If SMSS > 2190 bytes:
	//     IW = 2 * SMSS bytes and MUST NOT be more than 2 segments
	// If (SMSS > 1095 bytes) and (SMSS <= 2190 bytes):
	//     IW = 3 * SMSS bytes and MUST NOT be more than 3 segments
	// If SMSS <= 1095 bytes:
	//     IW = 4 * SMSS bytes and MUST NOT be more than 4 segments
	if (s->snd_mss > 2190)
		s->snd_cwnd = 2*s->snd_mss;
	else if (s->snd_mss > 1095)
		s->snd_cwnd = 3*s->snd_mss;
	else
		s->snd_cwnd = 4*s->snd_mss;
	// Force initial window to be equal to 10 SMSS
	s->snd_cwnd = 10*s->snd_mss;

	// set the value of initial cwnd
#ifdef BBR_ENABLED
	s->bbr->initial_cwnd = s->snd_cwnd;
#endif
	// The initial value of ssthresh SHOULD be set arbitrarily high (e.g.,
	// to the size of the largest possible advertised window), but ssthresh
	// MUST be reduced in response to congestion.
	//
	// Since the window is not scaled in SYN packets, TCPNewRenoAck does this

	if (TCPInfo::verbose())
		click_chatter("%s: syn, %s", class_name(), s->unparse_cong().c_str());
		
	// As specified in [RFC3390], the SYN/ACK and the acknowledgment of the
	// SYN/ACK MUST NOT increase the size of the congestion window.

	return p;
}

void
TCPNewRenoSyn::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPNewRenoSyn::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPNewRenoSyn)
