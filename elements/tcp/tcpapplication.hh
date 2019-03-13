/*
 * tcpapplication.{cc,hh} -- a TCP application
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

#ifndef CLICK_TCPAPPLICATION_HH
#define CLICK_TCPAPPLICATION_HH
#include <click/element.hh>
#include <click/string.hh>
#include "tcpsocket.hh"
CLICK_DECLS

class TCPApplication : public Element { public:

	TCPApplication() CLICK_COLD;

	const char *class_name() const { return "TCPApplication"; }

	// Socket API
	inline int click_socket(int domain, int type, int protocol);
	inline int click_fcntl(int sockfd, int cmd, int arg);
	inline int click_fcntl(int sockfd, int cmd);
	inline int click_bind(int sockfd, IPAddress &addr, uint16_t &port);
	inline int click_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	inline int click_getsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	inline int click_listen(int sockfd, int backlog);
	inline int click_accept(int sockfd, IPAddress &addr, uint16_t &port);
	inline int click_connect(int sockfd, IPAddress addr, uint16_t port);
	inline int click_send(int sockfd, const char *msg, size_t len);
	inline int click_recv(int sockfd, char *msg, size_t len);
	inline int click_close(int sockfd);
	inline int click_fsync(int sockfd);

	// Zero-copy (ZC) API
	inline int click_push(int sockfd, Packet *p);
	inline Packet *click_pull(int sockfd, int npkts = 1);
	
	//State modifications
	inline void click_set_task(int sockfd, BlockingTask * t);


#if HAVE_DPDK
	//RSS API
	static uint16_t click_rss_sport(IPFlowID flow);
	static uint32_t click_rss_hash(IPFlowID flow);
#endif	

	// Event handling API
#if HAVE_ALLOW_POLL
	inline int click_poll(struct pollfd *fds, size_t nfds, int timeout);
#endif
#if HAVE_ALLOW_EPOLL
	inline int click_epoll_create(int size);
	inline int click_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
	inline int click_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
	inline int click_epoll_close(int epfd);
#endif

  protected:

	// Helper functions
	String unparse_events(uint16_t events);
	Vector<String> unparse_pollfds(Vector<struct pollfd> &pollfds);

	int _pid;
	static int pid_counter;
};

inline int
TCPApplication::click_socket(int domain, int type, int protocol)
{
	return TCPSocket::socket(_pid, domain, type, protocol);
}

inline int 
TCPApplication::click_fcntl( int sockfd, int cmd,  int arg){
	return TCPSocket::fcntl(_pid, sockfd, cmd, arg);
}

inline int 
TCPApplication::click_fcntl( int sockfd, int cmd){
	return TCPSocket::fcntl(_pid, sockfd, cmd);
}

inline int
TCPApplication::click_bind(int sockfd, IPAddress &addr, uint16_t &port)
{
	return TCPSocket::bind(_pid, sockfd, addr, port);
}

inline int
TCPApplication::click_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
	return TCPSocket::setsockopt(_pid, sockfd, level, optname, optval, optlen);
}

inline int
TCPApplication::click_getsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
	return TCPSocket::getsockopt(_pid, sockfd, level, optname, optval, optlen);
}

inline int
TCPApplication::click_listen(int sockfd, int backlog)
{
	return TCPSocket::listen(_pid, sockfd, backlog);
}

inline int
TCPApplication::click_accept(int sockfd, IPAddress &addr, uint16_t &port)
{
	return TCPSocket::accept(_pid, sockfd, addr, port);
}

inline int
TCPApplication::click_connect(int sockfd, IPAddress addr, uint16_t port)
{
	return TCPSocket::connect(_pid, sockfd, addr,port);
}

inline int
TCPApplication::click_send(int sockfd, const char *msg, size_t len)
{
	return TCPSocket::send(_pid, sockfd, msg, len);
}

inline int
TCPApplication::click_recv(int sockfd, char *msg, size_t len)
{
	return TCPSocket::recv(_pid, sockfd, msg, len);
}

inline int
TCPApplication::click_close(int sockfd)
{
	return TCPSocket::close(_pid, sockfd);
}

inline int
TCPApplication::click_fsync(int sockfd)
{
	return TCPSocket::fsync(_pid, sockfd);
}

inline int
TCPApplication::click_push(int sockfd, Packet *p)
{
	TCPSocket::push(_pid, sockfd, p);
}

inline Packet *
TCPApplication::click_pull(int sockfd, int npkts)
{
	return TCPSocket::pull(_pid, sockfd, npkts);
}

inline void 
TCPApplication::click_set_task(int sockfd, BlockingTask * t)
{
	TCPSocket::set_task(_pid, sockfd, t);
}


#if HAVE_ALLOW_POLL
inline int
TCPApplication::click_poll(struct pollfd *fds, size_t nfds, int timeout)
{
	return TCPSocket::poll(_pid, fds, nfds, timeout);
}
#endif /* HAVE_ALLOW_POLL */

#if HAVE_ALLOW_EPOLL
inline int
TCPApplication::click_epoll_create(int size)
{
	return TCPSocket::epoll_create(_pid, size);
}

inline int
TCPApplication::click_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	return TCPSocket::epoll_ctl(_pid, epfd, op, fd, event);
}

inline int
TCPApplication::click_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	return TCPSocket::epoll_wait(_pid, epfd, events, maxevents, timeout);
}

inline int
TCPApplication::click_epoll_close(int epfd)
{
	return TCPSocket::epoll_close(_pid, epfd);
}
#endif /* HAVE_ALLOW_EPOLL */

#if HAVE_DPDK
inline uint16_t 
TCPApplication::click_rss_sport(IPFlowID flow)
{
	return TCPSocket::rss_sport(flow);
}

inline uint32_t 
TCPApplication::click_rss_hash(IPFlowID flow)
{
	return TCPSocket::rss_hash(flow);
}
#endif /* HAVE_DPDK*/

CLICK_ENDDECLS
#endif
