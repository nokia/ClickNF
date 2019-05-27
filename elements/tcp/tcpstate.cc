/*
 * tcpstate.{cc,hh} -- the TCP state
 * Rafael Laufer, Massimo Gallo, Myriana Rifai
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
#include <click/integers.hh>
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "tcptrimpacket.hh"
CLICK_DECLS

//static DPDKAllocator *pool[CLICK_CPU_MAX] = { 0 };
static TCPHashAllocator *pool[CLICK_CPU_MAX] = { 0 };

TCPState::TCPState(const IPFlowID &f) 
  : _hashnext(NULL),
    flow(f),
    state(TCP_CLOSED),
    snd_sack_permitted(false),
    snd_ts_ok(false), 
    snd_wscale_ok(false),
    snd_reinitialize_timer(false),
    is_passive(false),
    snd_wscale(0),
    rcv_wscale(0),
    snd_mss(TCP_SND_MSS_MIN),
    rcv_mss(TCP_RCV_MSS_DEFAULT), 
    acq_size(0),
    backlog(0),
    snd_nxt(0),
    rcv_nxt(0),
    rcv_wnd(0),
    acq_next(this),
    acq_prev(this),
    snd_wnd(0),
    snd_wl1(0),
    snd_wl2(0),
    snd_wnd_max(0),
    snd_cwnd(0xFFFFFFFF),
    snd_ssthresh(0),
    snd_bytes_acked(0),
    snd_dupack(0),
    snd_recover(0),
    snd_parack(0),
    wait(TCP_WAIT_NOTHING),
    snd_una(0),
    snd_isn(0),
    snd_rto(TCP_RTO_INIT),
    task(NULL),
    parent(NULL),
    ts_recent(0),
    ts_offset(0),
    ts_last_ack_sent(0),
    ts_recent_update(0),
    snd_srtt(0),
    snd_rttvar(0),
    pid(-1),
    sockfd(-1),
    epfd(-1),
    flags(0),
    error(0),
    rs(new RateSample()),
    bbr(new BBRState(this)),	
#if HAVE_TCP_KEEPALIVE
    snd_keepalive_count(0),
#endif
    snd_rtx_count(0),
    event(NULL)
{
}

TCPState::~TCPState()
{
	stop_timers();
	flush_queues();
}

TCPState *
TCPState::allocate()
{
	unsigned c = click_current_cpu_id();
    if (!pool[c]) {
		pool[c] = new TCPHashAllocator(sizeof(TCPState));
		assert(pool[c]);
	}

	return reinterpret_cast<TCPState *>(pool[c]->allocate());
}

void
TCPState::deallocate(TCPState *s)
{
    unsigned c = click_current_cpu_id();
    if (pool[c])
		pool[c]->deallocate(s);
}

bool
TCPState::clean_rtx_queue(uint32_t ack, bool verbose)
{
	bool removed = false;

	// Get RTX queue head
	Packet *p = rtxq.front();
	
	while (p) {
		const click_ip *ip = p->ip_header();
		const click_tcp *th = p->tcp_header();

		uint32_t seq = TCP_SEQ(th);
		uint32_t end = TCP_END(ip, th);

		// If ACK does not fully acknowledge the packet, get out
		if (unlikely(SEQ_GEQ(end, ack))) {
			// Trim beginning of the packet if receiver already got part of it
			if (unlikely(SEQ_LT(seq, ack))) {
				rtxq.pop_front();
				p = TCPTrimPacket::trim_begin(p, ack - seq);
				rtxq.push_front(p);
				removed = true;
			}
			break;
		}

		if (verbose)
			click_chatter("TCPState: remove seq space %u:%u(%u, %u, %u)", \
			     seq, end + 1, TCP_SNS(ip, th), p->length(), ntohs(ip->ip_len));

		rtxq.pop_front();
		p->kill();
		p = rtxq.front();

		removed = true;
	}

	// RFC 6298:
	//
	// "The following is the RECOMMENDED algorithm for managing the
	//  retransmission timer:
	//
	//  (5.1) ...
	//
	//	(5.2) When all outstanding data has been acknowledged, turn off the
	//        retransmission timer.
	//
	//  (5.3) When an ACK is received that acknowledges new data, restart the
	//        retransmission timer so that it will expire after RTO seconds
	//        (for the current value of RTO).
	if (rtxq.empty()) {
		rtx_timer.unschedule();
		wake_up(TCP_WAIT_RTXQ_EMPTY);
	}
	else if (removed)
		rtx_timer.schedule_after_msec(snd_rto);

	return removed;
}

int
TCPState::wait_event(int event)
{
//	click_assert(lock.locked());

	// Return value
	int ret = 0;

	while (true) {
		// If event occurred, get out
		if (wait_event_check(event))
			break;

		// If about to sleep and socket is nonblocking, notify error
		if (flags & SOCK_NONBLOCK) {
			ret = EAGAIN;
			break;
		}
		
		// Save the event
		wait = event;
		
		// Unschedule task and yield the processor
		task->unschedule();
		task->yield(true);

		// If some error occurred while sleeping, notify error
		if (error) {
			ret = error;
			break;
		}
	}

	// If not monitored by epoll, clear events for which state is waiting
	if (epfd < 0)
		wait = TCP_WAIT_NOTHING;

	return ret;
}

void
TCPState::notify_error(int e)
{
	error = e;
	if (epfd > 0){
		if (event == NULL){
			event = new TCPEvent(this, TCP_WAIT_ERROR);
			TCPInfo::epoll_eq_insert(pid, epfd, event);
		}
		else	
		    event->event |= TCP_WAIT_ERROR;
	}

	// Wake up if task is sleeping
	if (!task->scheduled())
		task->reschedule();
}

void
TCPState::wake_up(int ev)
{
	// Wake up task, if waiting for this event
	if (wait & ev) {
		if (epfd > 0){
			//If event requires to be treaded by epoll 
			if (ev | (TCP_WAIT_CLOSED | TCP_WAIT_FIN_RECEIVED | TCP_WAIT_RXQ_NONEMPTY | TCP_WAIT_ACQ_NONEMPTY | TCP_WAIT_TXQ_HALF_EMPTY | TCP_WAIT_CON_ESTABLISHED) ){
				if (event == NULL){
					event = new TCPEvent(this, ev);
					TCPInfo::epoll_eq_insert(pid, epfd, event);
				}
				else
					event->event |= ev;
			}
		}
		
		if (!task->scheduled())
			task->reschedule();
	}
}

bool
TCPState::wait_event_check(int ev)
{
	bool cond = false;

	// Check if one of the events we are waiting on actually occurred
	while (ev && !cond) {
		// Check one event at a time
		int e = ((ev & (ev - 1)) == 0 ? ev : 1 << (ffs_lsb((unsigned)ev) - 1));

		switch (e) {
		case TCP_WAIT_ACQ_NONEMPTY:
			cond |= !acq_empty();
			break;

		case TCP_WAIT_CON_ESTABLISHED:
			cond |= (state == TCP_ESTABLISHED);
			break;

		case TCP_WAIT_FIN_RECEIVED:
			cond |= (state == TCP_CLOSE_WAIT || \
			         state == TCP_LAST_ACK   || \
			         state == TCP_CLOSING    || \
			         state == TCP_TIME_WAIT);
			break;

		case TCP_WAIT_TXQ_HALF_EMPTY:
			cond |= (txq.bytes() < TCPInfo::wmem());
			break;

		case TCP_WAIT_RXQ_NONEMPTY:
			cond |= !rxq.empty();
			break;

		case TCP_WAIT_TXQ_EMPTY:
			cond |= txq.empty();
			break;

		case TCP_WAIT_RTXQ_EMPTY:
			cond |= rtxq.empty();
			break;

		default:
			// Invalid event
			click_chatter("TCPState: invalid event 0x%x to wait for", e);
			break;
		}

		// Toogle event bit off
		ev ^= e;
	}

	return cond;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PktQueue TCPBuffer)
ELEMENT_PROVIDES(TCPState)
