/*
 * tcpoptionsunparser.{cc,hh} -- Unparse TCP options
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


#ifndef CLICK_TCPOPTIONSUNPARSER_HH
#define CLICK_TCPOPTIONSUNPARSER_HH
#include <click/element.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

#define TCP_OPU_IN_SYN  0
#define TCP_OPU_IN_ACK  1
#define TCP_OPU_IN_RTX  2

#define TCP_OPU_OUT_ENQ 0
#define TCP_OPU_OUT_DPB 1
#define TCP_OPU_OUT_OUT 2

class TCPOptionsUnparser final : public Element { public:

	TCPOptionsUnparser() CLICK_COLD;

	const char *class_name() const		{ return "TCPOptionsUnparser"; }
	const char *port_count() const		{ return "3/3"; }
	const char *processing() const		{ return PUSH; }

	int initialize(ErrorHandler *);

	void push(int, Packet *) final;

  private:

	void handle_syn(Packet *);
	void handle_ack(Packet *);
	void handle_rtx(Packet *);
};

CLICK_ENDDECLS
#endif
