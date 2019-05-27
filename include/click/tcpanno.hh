/*
 * tcpanno.{cc,hh} -- TCP packet annotations
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

#include "packet_anno.hh"

#ifndef CLICK_TCPANNO_HH
#define CLICK_TCPANNO_HH
CLICK_DECLS

// Packet annotations for TCP
// NOTE! Not compatible with some Click annotations in packet_anno.hh

#define TCP_SOCKFD_ANNO_OFFSET    0 + DST_IP_ANNO_SIZE
#define TCP_SOCKFD_ANNO_SIZE      4 
#define TCP_SOCKFD_ANNO(p)        (p)->anno_u32(TCP_SOCKFD_ANNO_OFFSET)
#define SET_TCP_SOCKFD_ANNO(p, v) (p)->set_anno_u32(TCP_SOCKFD_ANNO_OFFSET, (v))

#define TCP_STATE_ANNO_OFFSET     4 + DST_IP_ANNO_SIZE
#define TCP_STATE_ANNO_SIZE       8
#define TCP_STATE_ANNO(p)        (TCPState*)(p)->anno_u64(TCP_STATE_ANNO_OFFSET)
#define SET_TCP_STATE_ANNO(p, v) (p)->set_anno_u64(TCP_STATE_ANNO_OFFSET, (v))

#define TCP_RTT_ANNO_OFFSET      12 + DST_IP_ANNO_SIZE
#define TCP_RTT_ANNO_SIZE         4
#define TCP_RTT_ANNO(p)          (p)->anno_u32(TCP_RTT_ANNO_OFFSET)
#define SET_TCP_RTT_ANNO(p, v)   (p)->set_anno_u32(TCP_RTT_ANNO_OFFSET, (v))

#define TCP_WND_ANNO_OFFSET      16 + DST_IP_ANNO_SIZE
#define TCP_WND_ANNO_SIZE         4
#define TCP_WND_ANNO(p)          (p)->anno_u32(TCP_WND_ANNO_OFFSET)
#define SET_TCP_WND_ANNO(p, v)   (p)->set_anno_u32(TCP_WND_ANNO_OFFSET, (v))

#define TCP_SEQ_ANNO_OFFSET      20 + DST_IP_ANNO_SIZE
#define TCP_SEQ_ANNO_SIZE         4
#define TCP_SEQ_ANNO(p)          (p)->anno_u32(TCP_SEQ_ANNO_OFFSET)
#define SET_TCP_SEQ_ANNO(p, v)   (p)->set_anno_u32(TCP_SEQ_ANNO_OFFSET, (v))

#define TCP_ACKED_ANNO_OFFSET    24 + DST_IP_ANNO_SIZE
#define TCP_ACKED_ANNO_SIZE       4
#define TCP_ACKED_ANNO(p)        (p)->anno_u32(TCP_ACKED_ANNO_OFFSET)
#define SET_TCP_ACKED_ANNO(p, v) (p)->set_anno_u32(TCP_ACKED_ANNO_OFFSET, (v))

#define TCP_MSS_ANNO_OFFSET      28 + DST_IP_ANNO_SIZE
#define TCP_MSS_ANNO_SIZE         2
#define TCP_MSS_ANNO(p)          (p)->anno_u16(TCP_MSS_ANNO_OFFSET)
#define SET_TCP_MSS_ANNO(p, v)   (p)->set_anno_u16(TCP_MSS_ANNO_OFFSET, (v))

#define TCP_OPLEN_ANNO_OFFSET    30 + DST_IP_ANNO_SIZE
#define TCP_OPLEN_ANNO_SIZE       1
#define TCP_OPLEN_ANNO(p)        (p)->anno_u8(TCP_OPLEN_ANNO_OFFSET)
#define SET_TCP_OPLEN_ANNO(p, v) (p)->set_anno_u8(TCP_OPLEN_ANNO_OFFSET, (v))

#define TCP_FLAGS_ANNO_OFFSET    31 + DST_IP_ANNO_SIZE
#define TCP_FLAGS_ANNO_SIZE       1
#define TCP_FLAGS_ANNO(p)        (p)->anno_u8(TCP_FLAGS_ANNO_OFFSET)
#define SET_TCP_FLAGS_ANNO(p, v) (p)->set_anno_u8(TCP_FLAGS_ANNO_OFFSET, (v))

#define TCP_FLAG_SACK      (1 << 0)  // SACKed packets
#define TCP_FLAG_ACK       (1 << 1)  // ACK needed
#define TCP_FLAG_MS        (1 << 2)  // More (buffered) segments coming
#define TCP_FLAG_SOCK_ADD  (1 << 3)  // Socket add
#define TCP_FLAG_SOCK_DEL  (1 << 4)  // Socket del
#define TCP_FLAG_SOCK_OUT  (1 << 5)  // Socket out
#define TCP_FLAG_SOCK_ERR  (1 << 6)  // Socket err
#define TCP_FLAG_ECE       (1 << 7)  // ECE needed

// Annotations valid only for App - TCPEpoll(Server, Client)
// NOTE May overwrite TCP Anno

#define TCP_DPORT_ANNO_OFFSET     4 + DST_IP_ANNO_SIZE
#define TCP_DPORT_ANNO_SIZE       2
#define TCP_DPORT_ANNO(p)          (p)->anno_u16(TCP_DPORT_ANNO_OFFSET)
#define SET_TCP_DPORT_ANNO(p, v)   (p)->set_anno_u16(TCP_DPORT_ANNO_OFFSET, (v))

// SACKed flag
#define TCP_SACK_FLAG_ANNO(p)          (TCP_FLAGS_ANNO(p) & TCP_FLAG_SACK)
#define SET_TCP_SACK_FLAG_ANNO(p)   \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SACK)
#define RESET_TCP_SACK_FLAG_ANNO(p) \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SACK))

// ACK needed flag
#define TCP_ACK_FLAG_ANNO(p)           (TCP_FLAGS_ANNO(p) & TCP_FLAG_ACK)
#define SET_TCP_ACK_FLAG_ANNO(p)    \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_ACK)
#define RESET_TCP_ACK_FLAG_ANNO(p)  \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_ACK))

// ECE needed flag
#define TCP_ECE_FLAG_ANNO(p)           (TCP_FLAGS_ANNO(p) & TCP_FLAG_ECE)
#define SET_TCP_ECE_FLAG_ANNO(p)    \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_ECE)
#define RESET_TCP_ECE_FLAG_ANNO(p)  \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_ECE))

// More (buffered) segments coming
#define TCP_MS_FLAG_ANNO(p)            (TCP_FLAGS_ANNO(p) & TCP_FLAG_MS)
#define SET_TCP_MS_FLAG_ANNO(p)    \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_MS)
#define RESET_TCP_MS_FLAG_ANNO(p)  \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_MS))

// Socket add. Signal a sockfd need to be / has been added  
#define TCP_SOCK_ADD_FLAG_ANNO(p)  (TCP_FLAGS_ANNO(p) & TCP_FLAG_SOCK_ADD)
#define SET_TCP_SOCK_ADD_FLAG_ANNO(p)    \
                  SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SOCK_ADD)
#define RESET_TCP_SOCK_ADD_FLAG_ANNO(p)  \
                  SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SOCK_ADD))

// Socket del. Signal a sockfd need to be / has been deleted  
#define TCP_SOCK_DEL_FLAG_ANNO(p)  (TCP_FLAGS_ANNO(p) & TCP_FLAG_SOCK_DEL)
#define SET_TCP_SOCK_DEL_FLAG_ANNO(p)    \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SOCK_DEL)
#define RESET_TCP_SOCK_DEL_FLAG_ANNO(p)  \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SOCK_DEL))
                 
// Socket out. Signal the sockfd is ready for sending data (i.e., is connected or has space left in the output buffer)
#define TCP_SOCK_OUT_FLAG_ANNO(p)  (TCP_FLAGS_ANNO(p) & TCP_FLAG_SOCK_OUT)
#define SET_TCP_SOCK_OUT_FLAG_ANNO(p)    \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SOCK_OUT)
#define RESET_TCP_SOCK_OUT_FLAG_ANNO(p)  \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SOCK_OUT))

// Socket err. Signal the sockfd has an error. 
#define TCP_SOCK_ERR_FLAG_ANNO(p)  (TCP_FLAGS_ANNO(p) & TCP_FLAG_SOCK_ERR)
#define SET_TCP_SOCK_ERR_FLAG_ANNO(p)    \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SOCK_ERR)
#define RESET_TCP_SOCK_ERR_FLAG_ANNO(p)  \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SOCK_ERR))
CLICK_ENDDECLS
#endif // CLICK_TCPANNO_HH
