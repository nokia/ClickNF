/*
 * tcpanno.{cc,hh} -- TCP packet annotations
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

#ifndef CLICK_TCPANNO_HH
#define CLICK_TCPANNO_HH
CLICK_DECLS

// Packet annotations for TCP

#define TCP_SOCKFD_ANNO_OFFSET    0
#define TCP_SOCKFD_ANNO_SIZE      4
#define TCP_SOCKFD_ANNO(p)        (p)->anno_u32(TCP_SOCKFD_ANNO_OFFSET)
#define SET_TCP_SOCKFD_ANNO(p, v) (p)->set_anno_u32(TCP_SOCKFD_ANNO_OFFSET, (v))

#define TCP_STATE_ANNO_OFFSET     4
#define TCP_STATE_ANNO_SIZE       8
#define TCP_STATE_ANNO(p)        (TCPState*)(p)->anno_u64(TCP_STATE_ANNO_OFFSET)
#define SET_TCP_STATE_ANNO(p, v) (p)->set_anno_u64(TCP_STATE_ANNO_OFFSET, (v))

#define TCP_RTT_ANNO_OFFSET      12
#define TCP_RTT_ANNO_SIZE         4
#define TCP_RTT_ANNO(p)          (p)->anno_u32(TCP_RTT_ANNO_OFFSET)
#define SET_TCP_RTT_ANNO(p, v)   (p)->set_anno_u32(TCP_RTT_ANNO_OFFSET, (v))

#define TCP_WND_ANNO_OFFSET      16
#define TCP_WND_ANNO_SIZE         4
#define TCP_WND_ANNO(p)          (p)->anno_u32(TCP_WND_ANNO_OFFSET)
#define SET_TCP_WND_ANNO(p, v)   (p)->set_anno_u32(TCP_WND_ANNO_OFFSET, (v))

#define TCP_SEQ_ANNO_OFFSET      20
#define TCP_SEQ_ANNO_SIZE         4
#define TCP_SEQ_ANNO(p)          (p)->anno_u32(TCP_SEQ_ANNO_OFFSET)
#define SET_TCP_SEQ_ANNO(p, v)   (p)->set_anno_u32(TCP_SEQ_ANNO_OFFSET, (v))

#define TCP_ACKED_ANNO_OFFSET    24
#define TCP_ACKED_ANNO_SIZE       4
#define TCP_ACKED_ANNO(p)        (p)->anno_u32(TCP_ACKED_ANNO_OFFSET)
#define SET_TCP_ACKED_ANNO(p, v) (p)->set_anno_u32(TCP_ACKED_ANNO_OFFSET, (v))

#define TCP_MSS_ANNO_OFFSET      28
#define TCP_MSS_ANNO_SIZE         2
#define TCP_MSS_ANNO(p)          (p)->anno_u16(TCP_MSS_ANNO_OFFSET)
#define SET_TCP_MSS_ANNO(p, v)   (p)->set_anno_u16(TCP_MSS_ANNO_OFFSET, (v))

#define TCP_OPLEN_ANNO_OFFSET    30
#define TCP_OPLEN_ANNO_SIZE       1
#define TCP_OPLEN_ANNO(p)        (p)->anno_u8(TCP_OPLEN_ANNO_OFFSET)
#define SET_TCP_OPLEN_ANNO(p, v) (p)->set_anno_u8(TCP_OPLEN_ANNO_OFFSET, (v))

#define TCP_FLAGS_ANNO_OFFSET    31
#define TCP_FLAGS_ANNO_SIZE       1
#define TCP_FLAGS_ANNO(p)        (p)->anno_u8(TCP_FLAGS_ANNO_OFFSET)
#define SET_TCP_FLAGS_ANNO(p, v) (p)->set_anno_u8(TCP_FLAGS_ANNO_OFFSET, (v))

#define TCP_FLAG_SACK      (1 << 0)  // SACKed packets
#define TCP_FLAG_ACK       (1 << 1)  // ACK needed
#define TCP_FLAG_MS        (1 << 2)  // More (buffered) segments coming
#define TCP_FLAG_SOCK_ADD  (1 << 3)  // Socket add
#define TCP_FLAG_SOCK_DEL  (1 << 4)  // Socket del

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

// More (buffered) segments coming
#define TCP_MS_FLAG_ANNO(p)            (TCP_FLAGS_ANNO(p) & TCP_FLAG_MS)
#define SET_TCP_MS_FLAG_ANNO(p)    \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_MS)
#define RESET_TCP_MS_FLAG_ANNO(p)  \
                     SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_MS))

// Socket add
#define TCP_SOCK_ADD_FLAG_ANNO(p)  (TCP_FLAGS_ANNO(p) & TCP_FLAG_SOCK_ADD)
#define SET_TCP_SOCK_ADD_FLAG_ANNO(p)    \
                  SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SOCK_ADD)
#define RESET_TCP_SOCK_ADD_FLAG_ANNO(p)  \
                  SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SOCK_ADD))

// Socket del
#define TCP_SOCK_DEL_FLAG_ANNO(p)  (TCP_FLAGS_ANNO(p) & TCP_FLAG_SOCK_DEL)
#define SET_TCP_SOCK_DEL_FLAG_ANNO(p)    \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) | TCP_FLAG_SOCK_DEL)
#define RESET_TCP_SOCK_DEL_FLAG_ANNO(p)  \
                 SET_TCP_FLAGS_ANNO(p, TCP_FLAGS_ANNO(p) & (~TCP_FLAG_SOCK_DEL))

CLICK_ENDDECLS
#endif // CLICK_TCPANNO_HH
