/*
 * tcp.{hh} -- generic definitions for TCP
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

#ifndef CLICK_TCP_HH
#define CLICK_TCP_HH

#include <click/config.h>
#include <click/packet.hh>
#include <click/pair.hh>
#include <clicknet/tcp.h>
CLICK_DECLS
/* From linux/socket.h */
#define SOL_TCP		6

#define HAVE_TCP_DELAYED_ACK 1

#define CLICK_DEBUG_TCP 0
#if CLICK_DEBUG_TCP
# define click_assert(x) assert(x)
#else
# define click_assert(x) ((void)(0))
#endif

// TCP SYN flag
inline bool
TCP_SYN(const click_tcp *th)
{
	click_assert(th);
	return (th->th_flags & TH_SYN);
}
inline bool
TCP_SYN(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_SYN(th);
}

// TCP RST flag
inline bool
TCP_RST(const click_tcp *th)
{
	click_assert(th);
	return (th->th_flags & TH_RST);
}
inline bool
TCP_RST(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_RST(th);
}

// TCP FIN flag
inline bool
TCP_FIN(const click_tcp *th)
{
	click_assert(th);
	return (th->th_flags & TH_FIN);
}
inline bool
TCP_FIN(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_FIN(th);
}

// TCP first sequence number
inline uint32_t
TCP_SEQ(const click_tcp *th)
{
	click_assert(th);
	return ntohl(th->th_seq);
}
inline uint32_t
TCP_SEQ(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_SEQ(th);
}

// TCP acknowledgment number
inline uint32_t
TCP_ACK(const click_tcp *th)
{
	click_assert(th);
	return ntohl(th->th_ack);
}
inline uint32_t
TCP_ACK(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_ACK(th);
}

// TCP window
inline uint16_t
TCP_WIN(const click_tcp *th)
{
	click_assert(th);
	return ntohs(th->th_win);
}
inline uint16_t
TCP_WIN(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_WIN(th);
}

// TCP data length
inline uint16_t
TCP_LEN(const click_ip *ip, const click_tcp *th)
{
	click_assert(ip && th);
	return ntohs(ip->ip_len) - ((ip->ip_hl + th->th_off) << 2);
}
inline uint16_t
TCP_LEN(Packet *p)
{
	click_assert(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	return TCP_LEN(ip, th);
}

// TCP sequence number space
inline uint16_t
TCP_SNS(const click_ip *ip, const click_tcp *th)
{
	return TCP_LEN(ip, th) + TCP_SYN(th) + TCP_FIN(th);
}
inline uint16_t
TCP_SNS(Packet *p)
{
	click_assert(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	return TCP_SNS(ip, th);
}

// TCP last sequence number
inline uint32_t
TCP_END(const click_ip *ip, const click_tcp *th)
{
	return TCP_SEQ(th) + TCP_SNS(ip, th) - 1;
}
inline uint32_t
TCP_END(Packet *p)
{
	click_assert(p);
	const click_ip *ip = p->ip_header();
	const click_tcp *th = p->tcp_header();
	return TCP_END(ip, th);
}

// TCP source port
inline uint32_t
TCP_SRC(const click_tcp *th)
{
	click_assert(th);
	return ntohs(th->th_sport);
}
inline uint32_t
TCP_SRC(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_SRC(th);
}

// TCP destination port
inline uint32_t
TCP_DST(const click_tcp *th)
{
	click_assert(th);
	return ntohs(th->th_dport);
}
inline uint32_t
TCP_DST(Packet *p)
{
	click_assert(p);
	const click_tcp *th = p->tcp_header();
	return TCP_DST(th);
}

// TCP maximum per-user and per-system sockets
#define TCP_USR_CAPACITY (1 << 12)
#define TCP_SYS_CAPACITY (1 << 20)

// TCP flow buckets in hash table
#define TCP_FLOW_BUCKETS  65536

// TCP flow timeout
#define TCP_FLOW_TIMEOUT  (   1800) // 1800 s

// TCP retransmission timeout
#define TCP_RTO_INIT      ( 1*1000) //    1 s
#define TCP_RTO_MIN       (    200) //  200 ms
#define TCP_RTO_MAX       (60*1000) //   60 s

// TCP max retransmission count
#define TCP_RTX_MAX       (      5) // 5 retransmissions

// TCP delayed ACK timeout
#define TCP_DELAYED_ACK   (    500) //  500 ms

// TCP keepalive timeout
#define TCP_KEEPALIVE     (75*1000) //   75 s

// TCP max keepalive count
#define TCP_KEEPALIVE_MAX (      9) // 9 keepalive packets

// TCP maximum segment lifetime (MSL)
//#define TCP_MSL          (120*1000) // 120 s
#define TCP_MSL           (    250) //  250 ms

// TCP maximum segment size (MSS)
//
// RFC 1122:
// "If an MSS option is not received at connection setup, TCP MUST
//  assume a default send MSS of 536 (576-40)."
#define TCP_SND_MSS_MIN       536
#define TCP_SND_MSS_MAX      1460
#define TCP_RCV_MSS_DEFAULT  1460

// TCP read(rx)/write(tx) memory size
#define TCP_RMEM_SHIFT_DEFAULT   20  //   1 MB
#define TCP_WMEM_SHIFT_DEFAULT   20  //   1 MB
#define TCP_RMEM_SHIFT_MIN       17  // 128 KB
#define TCP_WMEM_SHIFT_MIN       17  // 128 KB
#define TCP_RMEM_SHIFT_MAX       23  //   8 MB
#define TCP_WMEM_SHIFT_MAX       23  //   8 MB
#define TCP_RMEM_DEFAULT  (1 << TCP_RMEM_SHIFT_DEFAULT)
#define TCP_WMEM_DEFAULT  (1 << TCP_WMEM_SHIFT_DEFAULT)
#define TCP_RMEM_MIN      (1 << TCP_RMEM_SHIFT_MIN)
#define TCP_WMEM_MIN      (1 << TCP_WMEM_SHIFT_MIN)
#define TCP_RMEM_MAX      (1 << TCP_RMEM_SHIFT_MAX)
#define TCP_WMEM_MAX      (1 << TCP_WMEM_SHIFT_MAX)

// TCP window scaling default
#define TCP_RCV_WSCALE_DEFAULT  (TCP_RMEM_SHIFT_DEFAULT - 15)

// Packet header length in TCP packets
#define TCP_HEADROOM \
  ((sizeof(click_ether) + sizeof(click_ip) + sizeof(click_tcp) + 40 + 3) & (~3))

CLICK_ENDDECLS
#endif
