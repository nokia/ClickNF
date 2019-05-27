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

#ifndef CLICK_TCPSTATE_HH
#define CLICK_TCPSTATE_HH
#include <click/timestamp.hh>
#include <click/ipflowid.hh>
#include <click/packet.hh>
#include <click/timer.hh>
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/tcpanno.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/tcp.hh>
#include <clicknet/ether.h>
#include "bbr/bbrstate.hh"
#include "bbr/ratesample.hh"
#include "tcphashallocator.hh"
#include "pktqueue.hh"
#include "blockingtask.hh"
#include "tcplist.hh"
#include "tcpbuffer.hh"
#include "tcptimer.hh"
#include "tcpeventqueue.hh"
#include "util.hh"
CLICK_DECLS

class TCPFlowTable;
class BBRState;
class RateSample;

const uint8_t TCP_CLOSED      =  0;
const uint8_t TCP_LISTEN      =  1;
const uint8_t TCP_SYN_SENT    =  2;
const uint8_t TCP_SYN_RECV    =  3;
const uint8_t TCP_ESTABLISHED =  4;
const uint8_t TCP_FIN_WAIT1   =  5;
const uint8_t TCP_FIN_WAIT2   =  6;
const uint8_t TCP_CLOSING     =  7;
const uint8_t TCP_TIME_WAIT   =  8;
const uint8_t TCP_CLOSE_WAIT  =  9;
const uint8_t TCP_LAST_ACK    = 10;

const uint8_t SOCK_LINGER     =  1;

const uint16_t TCP_WAIT_NOTHING          = (0 << 0);
const uint16_t TCP_WAIT_ACQ_NONEMPTY     = (1 << 0);
const uint16_t TCP_WAIT_CON_ESTABLISHED  = (1 << 1);
const uint16_t TCP_WAIT_FIN_RECEIVED     = (1 << 2);
const uint16_t TCP_WAIT_TXQ_EMPTY        = (1 << 3);
const uint16_t TCP_WAIT_TXQ_HALF_EMPTY   = (1 << 4);
const uint16_t TCP_WAIT_RXQ_NONEMPTY     = (1 << 5);
const uint16_t TCP_WAIT_RTXQ_EMPTY       = (1 << 6);
const uint16_t TCP_WAIT_CLOSED           = (1 << 7);
const uint16_t TCP_WAIT_ERROR            = (1 << 8);

class TCPState { public:

	TCPState(const IPFlowID &);
	~TCPState();

	inline bool is_acceptable_seq(Packet *p) const;
	inline bool is_acceptable_seq(uint32_t seq, uint32_t len) const;
	inline bool is_acceptable_ack(Packet *p) const;
	inline bool is_acceptable_ack(uint32_t ack) const;
	inline bool bound() const;

	inline uint32_t available_tx_window() const;
	inline uint32_t available_rx_window() const;

	inline void flush_queues();
	inline void stop_timers();

	inline int unparse(char *s) const;
	inline String unparse() const;

	inline String unparse_cong() const;

	bool clean_rtx_queue(uint32_t ack, bool verbose = false);
	int wait_event(int event);
	bool wait_event_check(int event);
	inline void wait_event_set(int event) { wait |= event; }
	inline void wait_event_reset() { wait = TCP_WAIT_NOTHING;} //NOTE Might neet to clean active events here... TCPInfo::epoll_eq_erase(pid,epfd, event); free(event);}
	void wake_up(int event);
	void notify_error(int);

	inline uint32_t tcp_packets_in_flight();
	inline void acq_push_back(TCPState *s);
	inline void acq_erase(TCPState *s);
	inline void acq_detach();
	inline TCPState *acq_front();
	inline bool acq_empty();
	inline void acq_pop_front();

	// To be used in the flow hash table
	typedef IPFlowID key_type;
	typedef const IPFlowID & key_const_reference;
	inline key_const_reference hashkey() const {
		return flow;
	}

//TODO Check alignement
	TCPState *_hashnext;
//	TCPList_member list;                // list head
	IPFlowID flow;                      // flow tuple
	uint8_t  state;                     // TCP state
	uint8_t  snd_sack_permitted:1,      // SACK option ok
	         snd_ts_ok:1,               // timestamp option ok
	         snd_wscale_ok:1,           // window scaling ok
	         snd_reinitialize_timer:1,  // restart RTX timer after established
	         is_passive:1,              // true if socket comes from a server
	         unused1:1,
	         unused2:1,
	         unused3:1;

	uint8_t  snd_wscale;                // send window scaling
	uint8_t  rcv_wscale;                // recv window scaling

	uint16_t snd_mss;                   // send maximum segment size
	uint16_t rcv_mss;                   // recv maximum segment size

	int acq_size;
	int backlog;

	uint32_t snd_nxt;                   // send next

	// Receive sequence space
	//                       1          2          3      
	//                   ----------|----------|---------- 
	//                          RCV.NXT    RCV.NXT        
	//                                    +RCV.WND        
	uint32_t rcv_nxt;                   // receive next
	uint32_t rcv_wnd;                   // receive window
	
	TCPState *acq_next;                 // next TCB in accept queue
	TCPState *acq_prev;                 // prev TCB in accept queue
	
	uint32_t snd_wnd;                   // send window
	uint32_t snd_wl1;                   // seqno used for last window update
	uint32_t snd_wl2;                   // ackno used for last window update
	uint32_t snd_wnd_max;               // send window maximum

	uint32_t snd_cwnd;                  // congestion window
	uint32_t snd_ssthresh; 	            // slow start threshold
	uint32_t snd_bytes_acked;           // bytes acked in the cwnd
	uint16_t snd_dupack;                // number of consecutive dup acks
	uint32_t snd_recover;               // last segment when 3rd DUPACK arrives
	uint16_t snd_parack;                // partial ACK counter
   	uint16_t wait;                      // what is this socket waiting on
	// Send sequence space
	//                    1         2          3          4      
	//              ----------|----------|----------|---------- 
	//                     SND.UNA    SND.NXT    SND.UNA        
	//                                          +SND.WND      
	uint32_t snd_una;                   // send unacknowledged
	uint32_t snd_isn;                   // initial sequence number
	uint32_t snd_rto;                   // retransmission timeout

	BlockingTask *task;                 // calling (blocking) task
	TCPState *parent;                   // parent TCB (if passive)

	uint32_t ts_recent;                 // timestamp recent
	uint32_t ts_offset;                 // timestamp offset
	uint32_t ts_last_ack_sent;          // timestamp last ACK sent
	uint32_t ts_recent_update;          // timestamp when recent was updateda

	uint32_t snd_srtt;                  // smoothed RTT
	uint32_t snd_rttvar;                // RTT variance

	TCPBuffer rxb;                      // RX buffer
	PktQueue  rxq;                      // RX queue

	PktQueue  txq;                      // TX queue
	PktQueue rtxq;                      // RTX queue


	int pid;
	int sockfd;
	int epfd;
	int flags; // NOTE: NONBLOCK is in reality a FD flag (O_RDWR)	
	int error;

		/**
	 * DCTCP state variables
	 */
	double alpha = 1;
	uint32_t bytes_acked = 0;
	uint32_t window_end = snd_una;
	uint32_t bytes_marked = 0;
	double gain = 0.0625;
	bool ce = false;
	/**
	 * END DCTCP
	 */

	/**
	 * BBR state variable
	 */
#define BBR_ENABLED 1
#define TCPCB_SACKED_ACKED	0x01	/* SKB ACK'd by a SACK block	*/
#define TCPCB_SACKED_RETRANS	0x02	/* SKB retransmitted		*/
#define TCPCB_LOST		0x04	/* SKB is lost			*/
#define TCPCB_TAGBITS		0x07	/* All tag bits			*/
#define TCPCB_REPAIRED		0x10	/* SKB repaired (no skb_mstamp_ns)	*/
#define TCPCB_EVER_RETRANS	0x80	/* Ever retransmitted frame	*/
#define TCPCB_RETRANS		(TCPCB_SACKED_RETRANS|TCPCB_EVER_RETRANS| \
				TCPCB_REPAIRED)
#if BBR_ENABLED
	uint32_t 	delivered,
				rate_delivered,
				rate_app_limited,
				app_limited,
				last_rtt;
	uint64_t 	delivered_ustamp,
				rate_interval_us,
				first_sent_time;
	uint8_t		sacked:1;
	RateSample *rs;
	BBRState *bbr;
	uint64_t next_send_time = 0;
	TCPTimer tx_timer;
#endif
	/**
	 * End BBR state variable
	 */
#if HAVE_TCP_KEEPALIVE
	uint16_t  snd_keepalive_count;       // number of keepalives without ACK
#endif
	uint16_t  snd_rtx_count;             // number of retx for the HOL packet

	uint8_t  bind_address_no_port:1,     // disable port binding when port = 0 
	         unused4:1,  
	         unused5:1,  
	         unused6:1,  
	         unused7:1,
	         unused8:1,
		 unused9:1,
	         unused10:1;

	TCPEvent* event; 

	TCPTimer rtx_timer; //CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
#if HAVE_TCP_KEEPALIVE
	TCPTimer keepalive_timer; //CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
#endif
#if HAVE_TCP_DELAYED_ACK
	TCPTimer delayed_ack_timer;// CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
#endif


//	RCUSpinlock lock;
	static TCPState *allocate();
	static void deallocate(TCPState *);
};/* CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);*/ //NOTE causes seg. fault in initialization of a state after TCPInfo::allocate()

inline uint32_t TCPState::tcp_packets_in_flight(){
	 return rtxq.packets() - snd_rtx_count - rxb.sack().blocks() -rxq.packets();
}

inline void
TCPState::acq_push_back(TCPState *s)
{
	s->acq_next = this;
	s->acq_prev = acq_prev;
	acq_prev->acq_next = s;
	acq_prev = s;
	acq_size++;
}

inline void
TCPState::acq_erase(TCPState *s)
{
	s->acq_detach();
	acq_size--;
}

inline void
TCPState::acq_detach()
{
	acq_prev->acq_next = acq_next;
	acq_next->acq_prev = acq_prev;
	acq_next = acq_prev = this;
}

inline TCPState *
TCPState::acq_front()
{
	return acq_next;
}

inline bool
TCPState::acq_empty()
{
	return acq_size == 0;
}

inline void
TCPState::acq_pop_front()
{
	acq_erase(acq_next);
}

inline uint32_t
TCPState::available_tx_window() const
{
	uint32_t frecovery = (snd_dupack <= 2 ? snd_dupack*snd_mss : 0);
	uint32_t tx_window = MIN(snd_cwnd + frecovery, snd_wnd);
	uint32_t in_flight = snd_nxt - snd_una;

	return (tx_window > in_flight ? tx_window - in_flight : 0);
}

inline uint32_t
TCPState::available_rx_window() const
{
	uint32_t in_buffer = rxq.bytes() + rxb.bytes();
	return (rcv_wnd > in_buffer ? rcv_wnd - in_buffer : 0);
}

// RFC 793:
// "There are four cases for the acceptability test for an incoming
//  segment:
//
//  Segment Receive  Test
//  Length  Window
//  ------- -------  -------------------------------------------
//
//     0       0     SEG.SEQ = RCV.NXT
//
//     0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
//
//    >0       0     not acceptable
//
//    >0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
//                   or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND"
//
inline bool
TCPState::is_acceptable_seq(uint32_t seq, uint32_t len) const
{
	uint32_t end;

	if (len == 0) {
		if (unlikely(rcv_wnd == 0))
			return (seq == rcv_nxt);
		else
			return (SEQ_LEQ(rcv_nxt, seq) && SEQ_LT(seq, rcv_nxt + rcv_wnd));
	}
	else {
		if (unlikely(rcv_wnd == 0)) 
			return false;
		else {
			end = seq + len - 1;
			return ((SEQ_LEQ(rcv_nxt, seq) && SEQ_LT(seq, rcv_nxt + rcv_wnd)) ||
			        (SEQ_LEQ(rcv_nxt, end) && SEQ_LT(end, rcv_nxt + rcv_wnd)));
		}
	}
}

inline bool
TCPState::is_acceptable_seq(Packet *p) const
{
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();

	return is_acceptable_seq(TCP_SEQ(th), TCP_SNS(ip, th));
}

inline bool
TCPState::is_acceptable_ack(uint32_t ack) const
{
	return (SEQ_LT(snd_una, ack) && SEQ_LEQ(ack, snd_nxt));
}   

inline bool
TCPState::is_acceptable_ack(Packet *p) const
{
	const click_tcp *th = p->tcp_header();
	return is_acceptable_ack(TCP_ACK(th));
}

inline bool
TCPState::bound() const
{
	return flow.saddr() && flow.sport();
}

inline void
TCPState::flush_queues()
{
	txq.flush();
	rxq.flush();
	rtxq.flush();
}

inline void
TCPState::stop_timers()
{
	rtx_timer.unschedule();
//	tw_timer.unschedule();
#if HAVE_TCP_KEEPALIVE
	keepalive_timer.unschedule();
#endif
#if HAVE_TCP_DELAYED_ACK
	delayed_ack_timer.unschedule();
#endif
}

inline String
TCPState::unparse_cong() const
{
	StringAccum sa;
	sa << "cwnd " << snd_cwnd << ", ssthresh " << snd_ssthresh;
	return sa.take_string();
}

inline int
TCPState::unparse(char *s) const
{
	if (s) {
		StringAccum sa;
		sa << flow.unparse() << " ";
		
		switch (state) {
		case TCP_CLOSED:
			sa << "CLOSED";
			break;
		case TCP_LISTEN:
			sa << "LISTEN";
			break;
		case TCP_SYN_SENT:
			sa << "SYN_SENT";
			break;
		case TCP_SYN_RECV:
			sa << "SYN_RECV";
			break;
		case TCP_ESTABLISHED:
			sa << "ESTABLISHED";
			break;
		case TCP_FIN_WAIT1:
			sa << "FIN_WAIT1";
			break;
		case TCP_FIN_WAIT2:
			sa << "FIN_WAIT2";
			break;
		case TCP_CLOSING:
			sa << "CLOSING";
			break;
		case TCP_TIME_WAIT:
			sa << "TIME_WAIT";
			break;
		case TCP_CLOSE_WAIT:
			sa << "CLOSE_WAIT";
			break;
		case TCP_LAST_ACK:
			sa << "LAST_ACK";
			break;
		default:
			sa << "ERROR";
			break;
		}
		return sprintf(s,"%s", sa.take_string().c_str());
	}
	return 0;
}

inline String
TCPState::unparse() const
{
    char tmp[64];
    return String(tmp, unparse(tmp));
}

CLICK_ENDDECLS
#endif // CLICK_TCPSTATE_HH
