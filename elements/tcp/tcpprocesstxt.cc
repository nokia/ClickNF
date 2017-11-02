/*
 * tcpprocesstxt.{cc,hh} -- Process data in TCP packet
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
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpprocesstxt.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "util.hh"
CLICK_DECLS

TCPProcessTxt::TCPProcessTxt()
{
}

Packet *
TCPProcessTxt::smaction(Packet *p)
{
	TCPState *s = TCP_STATE_ANNO(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	click_assert(s && ip && th);

	// Get data length
	uint16_t len = TCP_LEN(ip, th);

	// RFC 793:
	// "sixth, check the URG bit (ignored)"
	//
	// "seventh, process the segment text"
	switch (s->state) {
	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
		// "Once in the ESTABLISHED state, it is possible to deliver segment
		//  text to user RECEIVE buffers.  Text from segments can be moved
		//  into buffers until either the buffer is full or the segment is
		//  empty.  If the segment empties and carries an PUSH flag, then
		//  the user is informed, when the buffer is returned, that a PUSH
		//  has been received.
		//
		//  When the TCP takes responsibility for delivering the data to the
		//  user it must also acknowledge the receipt of the data.
		//
		//  Once the TCP takes responsibility for the data it advances
		//  RCV.NXT over the data accepted, and adjusts RCV.WND as
		//  apporopriate to the current buffer availability.  The total of
		//  RCV.NXT and RCV.WND should not be reduced.
		//
		//  Please note the window management suggestions in section 3.7.
		//
		//  Send an acknowledgment of the form:
		//
		//    <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
		//
		//  This acknowledgment should be piggybacked on a segment being
		//  transmitted if possible without incurring undue delay."

		// Make sure sequence number is correct
		click_assert(TCP_SEQ(th) == s->rcv_nxt);

		// If packet has data, add it to RX queue
		if (len > 0) {
			// Break segment chain for cloned packet (only header needed)
			Packet *q = p->seg_split();

			// Clone packet
			Packet *c = p->clone();

			// Concatenate segment chain again
			if (q)
				p->seg_join(q);

			// Strip IP/TCP headers of the original packet
			p->pull((ip->ip_hl + th->th_off) << 2);

			// Insert original packet into RX queue, one segment at a time
			while (p) {
				q = p->seg_split();
				s->rxq.push_back(p);
				p = q;
			}

			// Adjust receive variables
			s->rcv_nxt += len;
			s->rcv_wnd -= len;

			// If more buffered segments are coming, do not send an ACK
			if (!TCP_MS_FLAG_ANNO(c)) {
#if HAVE_TCP_DELAYED_ACK
				// If ACK flag is set, this is the last segment of a batch that
				// fills a gap, send ACK acknowledging everything immmediately
				if (TCP_ACK_FLAG_ANNO(c) || s->delayed_ack_timer.scheduled() ||
				    len >= ((s->rcv_mss - (s->snd_ts_ok ? 12 : 0)) << 1)) {
					s->delayed_ack_timer.unschedule();
					SET_TCP_ACK_FLAG_ANNO(c);
				}
				else {
					uint32_t timeout = MIN(TCP_DELAYED_ACK, TCP_RTO_MIN >> 1);
					Timestamp now = c->timestamp_anno();
					if (now) {
						Timestamp tmo = now +  Timestamp::make_msec(timeout);
						s->delayed_ack_timer.schedule_at_steady(tmo);
					}
					else 
						s->delayed_ack_timer.schedule_after_msec(timeout);
				}
#else
				// Without delayed ACK, send an ACK for every new data packet
				SET_TCP_ACK_FLAG_ANNO(c);
#endif
			}

			// Wake user task
			s->wake_up(TCP_WAIT_RXQ_NONEMPTY);

			// Send the clone to be slaughtered
			return c;
		}

		break;

	case TCP_CLOSE_WAIT:
	case TCP_CLOSING:
	case TCP_LAST_ACK:
	case TCP_TIME_WAIT:
		// "This should not occur, since a FIN has been received from the
		//  remote side.  Ignore the segment text."
		break;

	default:
		assert(0);
	}

	return p;
}

void
TCPProcessTxt::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
TCPProcessTxt::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPProcessTxt)
