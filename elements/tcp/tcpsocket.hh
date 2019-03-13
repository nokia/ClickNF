/*
 * tcpsocket.{cc,hh} -- TCP socket API
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

#ifndef CLICK_TCPSOCKET_HH
#define CLICK_TCPSOCKET_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/ipaddress.hh>
#include "tcpstate.hh"
#include <bits/socket.h>
#include <stdarg.h>


#if HAVE_DPDK
#include <rte_byteorder.h>
#include <rte_thash.h>
#include "../userlevel/dpdk.hh"
#endif

#if HAVE_ALLOW_POLL
# if CLICK_USERLEVEL && HAVE_POLL_H
#  include <poll.h>
# else
#  define POLLIN        0x0001
#  define POLLPRI       0x0002
#  define POLLOUT       0x0004
#  define POLLERR       0x0008
#  define POLLHUP       0x0010
#  define POLLNVAL      0x0020
struct pollfd {
	int fd;
	uint16_t events;
	uint16_t revents;
};
# endif /* CLICK_USERLEVEL && HAVE_POLL_H */
#endif /* HAVE_ALLOW_POLL */

#if HAVE_ALLOW_EPOLL
# define MAX_EPOLLFD 4096
# if CLICK_USERLEVEL && HAVE_SYS_EPOLL_H
#  include <sys/epoll.h>
# else
#  define EPOLLIN       0x0001
#  define EPOLLPRI      0x0002
#  define EPOLLOUT      0x0004
#  define EPOLLERR      0x0008
#  define EPOLLHUP      0x0010
#  define EPOLL_CTL_ADD      1   // Add a file descriptor
#  define EPOLL_CTL_DEL      2   // Remove a file descriptor
#  define EPOLL_CTL_MOD      3   // Change file descriptor
typedef union epoll_data
{
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;
struct epoll_event
{
	uint32_t events;
	epoll_data_t data;
} CLICK_SIZE_PACKED_ATTRIBUTE;
# endif /* CLICK_USERLEVEL && HAVE_SYS_EPOLL_H */
#endif /* HAVE_ALLOW_EPOLL */

CLICK_DECLS

#define TCP_SOCKET_OUT_SYN_PORT 0
#define TCP_SOCKET_OUT_RST_PORT 1
#define TCP_SOCKET_OUT_FIN_PORT 2
#define TCP_SOCKET_OUT_TXT_PORT 3
#define TCP_SOCKET_OUT_USR_PORT 4

class TCPSocket final : public Element { public:

	TCPSocket() CLICK_COLD;

	const char *class_name() const { return "TCPSocket"; }
	const char *port_count() const { return "1/5"; }
	const char *processing() const { return "h/hhhhh"; }

	int configure(Vector<String> &, ErrorHandler *);

	void add_handlers();
	inline void push(int, Packet *p) { p->kill(); };
	inline Packet *pull(int) { return NULL; };

	// Socket API
	static int socket(int pid, int domain, int type, int protocol);
	static int fcntl(int pid, int sockfd, int cmd, int arg);
	static int fcntl(int pid, int sockfd, int cmd);
	static int bind(int pid, int sockfd, IPAddress &addr, uint16_t &port);
	static int listen(int pid, int sockfd, int backlog);
	static int accept(int pid, int sockfd, IPAddress &addr, uint16_t &port);
	static int connect(int pid, int sockfd, IPAddress addr, uint16_t port);
	static int send(int pid, int sockfd, const char *msg, size_t len);
	static int recv(int pid, int sockfd, char *msg, size_t len);
	static int close(int pid, int sockfd);
	static int fsync(int pid, int sockfd);
	static int setsockopt(int pid, int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	static int getsockopt(int pid, int sockfd, int level, int optname, const void *optval, socklen_t optlen);

	// Zero-copy API
	static int push(int pid, int sockfd, Packet *p);
	static Packet *pull(int pid, int sockfd, int npkts = 1);
	
	//State modifications
	static int set_task(int pid, int sockfd, BlockingTask * t);

#if HAVE_DPDK
	//RSS API
	static int rss_sport(IPFlowID flow);
	static uint32_t rss_hash(IPFlowID flow);
#endif

	// Socket event handling API
#if HAVE_ALLOW_POLL
	static int poll(int pid, struct pollfd *fds, size_t nfds, int timeout);
#endif
#if HAVE_ALLOW_EPOLL
	static int epoll_create(int pid,int size);
	static int epoll_ctl(int pid, int epfd, int op, int fd, struct epoll_event *event);
	static int epoll_wait(int pid, int epfd, struct epoll_event *events, int maxevents, int timeout);
	static int epoll_close(int pid, int epfd);	
#endif

  private:
	static int __bind(TCPState *s, IPAddress &addr, uint16_t &port, bool bind_address_no_port);

	// Handlers
	static int h_socket(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_bind(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_listen(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_accept(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_connect(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_send(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_recv(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_close(int, String&, Element*, const Handler*, ErrorHandler*);
	static int h_fsync(int, String&, Element*, const Handler*, ErrorHandler*);

#if HAVE_ALLOW_EPOLL
	void epoll_init();
#endif

	static TCPSocket *_socket;
	uint16_t _nthreads;
		
	friend class TCPState;

};

CLICK_ENDDECLS
#endif

