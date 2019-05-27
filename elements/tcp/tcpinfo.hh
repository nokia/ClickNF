/*
 * tcpinfo.hh -- TCP information used in multiple modules
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

#ifndef CLICK_TCPINFO_HH
#define CLICK_TCPINFO_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/atomic.hh>
#include <click/ipaddress.hh>
#include <click/ipflowid.hh>
#include <click/hashcontainer.hh>
#include <click/master.hh>
#include <click/hashtable.hh>
#include "tcpstate.hh"
#include "tcpsocket.hh"
#include "tcpfdesc.hh"
#include "tcptable.hh"
#include "tcpeventqueue.hh"
#include "tcpflowtable.hh"
#include "tcpporttable.hh"
CLICK_DECLS

#define MAX_PIDS 4096

class TCPInfo final : public Element { public:

	TCPInfo() CLICK_COLD;

	const char *class_name() const	{ return "TCPInfo"; }
	int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

	// TCP info API
	static inline bool verbose();
	static inline uint32_t rmem();
	static inline uint32_t wmem();
	static inline uint32_t sys_capacity();
	static inline uint32_t sys_sockets();
	static inline void inc_sys_sockets();
	static inline void dec_sys_sockets();
	static inline uint32_t usr_capacity();
	static inline uint32_t usr_sockets(int);
	static inline void inc_usr_sockets(int);
	static inline void dec_usr_sockets(int);
	static inline const Vector<IPAddress> &addr();
	static inline uint32_t cong_control();

#if HAVE_ALLOW_EPOLL	
	typedef TCPTable<TCPEventQueue *> EpollTableThread;
	typedef EpollTableThread* EpollTable;
	typedef TCPFDesc* EpollFDesc;
#endif	
	static inline bool pid_valid(int);

	// Flow
	typedef TCPFlowTable* FlowTable;
	static inline TCPState *flow_lookup(const IPFlowID &flow);
	static inline int flow_insert(TCPState *s);
	static inline int flow_remove(TCPState *s);
	static inline int flow_remove(const IPFlowID &flow);

	// Port
	typedef TCPPortTable* PortTable;
	static inline void port_add(const IPAddress &a);
	static inline bool port_get(const IPAddress &a, uint16_t p, TCPState *s);
	static inline void port_put(const IPAddress &a, uint16_t p);
	static inline bool port_lookup(const IPAddress &a, uint16_t p);

	// Sock
	typedef TCPTable<TCPState *> TCPSockTable;
	typedef TCPSockTable* SockTable;
	typedef TCPFDesc* SockFDesc;
	typedef Vector<uint32_t> SockCount;
	static inline int  sock_get(int pid, TCPState *s);
	static inline void sock_put(int pid, int sockfd);
	static inline TCPState *sock_lookup(int pid, int sockfd);
	static inline bool sock_valid(int sockfd);

#if HAVE_ALLOW_EPOLL
	static inline int epoll_fd_get(int pid);
	static inline bool epoll_fd_valid(int epfd);
	static inline bool epoll_fd_exists(int pid, int epfd);
	static inline void epoll_fd_put(int pid, int epfd);

	static inline TCPEventQueue::iterator epoll_eq_begin(int pid, int epfd);	
	static inline TCPEventQueue::iterator epoll_eq_end(int pid, int epfd);
	static inline void epoll_eq_erase(int pid, int epfd, TCPEvent* ev);
// 	static inline TCPEvent* epoll_eq_pop_front(int pid, int epfd);
	static inline void epoll_eq_insert(int pid, int epfd, TCPEvent* tev);
	static inline int epoll_eq_size(int pid, int epfd);
#endif

  private:

	static bool _verbose;
	static bool _initialized;
	static uint32_t _rmem;
	static uint32_t _wmem;
	static uint32_t _usr_capacity;
	static thread_local SockCount _usr_sockets;
	static uint32_t _sys_capacity;
	static uint64_t _buckets;
	static thread_local uint32_t _sys_sockets;
	static Vector<IPAddress> _addr;
	static PortTable _portTable;
	static FlowTable _flowTable;
	static SockTable _sockTable;
	static SockFDesc _sockFDesc;
	static uint32_t _nthreads;
	static uint32_t _cong_control;
#if HAVE_ALLOW_EPOLL
	static EpollTable _epollTable;
	static EpollFDesc _epollFDesc;
#endif

};

inline bool
TCPInfo::verbose()
{
	return _verbose;
}

inline uint32_t
TCPInfo::rmem()
{
	return _rmem;
}

inline uint32_t
TCPInfo::wmem()
{
	return _wmem;
}

inline uint32_t
TCPInfo::sys_capacity()
{
	return _sys_capacity;
}

inline uint32_t
TCPInfo::sys_sockets()
{
	return _sys_sockets;
}

inline void
TCPInfo::inc_sys_sockets()
{
	_sys_sockets++;
}

inline void
TCPInfo::dec_sys_sockets()
{
	_sys_sockets--;
}

inline uint32_t
TCPInfo::usr_capacity()
{
	return _usr_capacity;
}

inline uint32_t
TCPInfo::usr_sockets(int pid)
{
	return _usr_sockets[pid];
}

inline void
TCPInfo::inc_usr_sockets(int pid)
{
	_usr_sockets[pid]++;
}

inline void
TCPInfo::dec_usr_sockets(int pid)
{
	_usr_sockets[pid]--;
}

inline const Vector<IPAddress> &
TCPInfo::addr()
{
	return _addr;
}

inline uint32_t TCPInfo::cong_control()
{
	return _cong_control;
}

inline TCPState *
TCPInfo::flow_lookup(const IPFlowID &flow)
{
	unsigned c = click_current_cpu_id();
	return _flowTable[c].lookup(flow);
}

inline int
TCPInfo::flow_insert(TCPState *s)
{
	unsigned c = click_current_cpu_id();
	return _flowTable[c].insert(s);
}

inline int
TCPInfo::flow_remove(TCPState *s)
{
	unsigned c = click_current_cpu_id();
	//Remove associated events with TCPState
	if ((s->event != NULL) && (s->epfd>0)){
		TCPInfo::epoll_eq_erase(s->pid, s->epfd, s->event);
		delete(s->event);
		s->event = NULL;
	}
	return _flowTable[c].remove(s);
}

inline bool
TCPInfo::port_get(const IPAddress &addr, uint16_t port, TCPState *s)
{
	unsigned c = click_current_cpu_id();
	return _portTable[c].get(addr, port, s);
}

inline bool 
TCPInfo::port_lookup(const IPAddress &addr, uint16_t port)
{
	unsigned c = click_current_cpu_id();
	return _portTable[c].lookup(addr, port);
}

inline void
TCPInfo::port_add(const IPAddress &addr)
{
	unsigned c = click_current_cpu_id();
	_portTable[c].add(addr);
}

inline void
TCPInfo::port_put(const IPAddress &addr, uint16_t port)
{
	unsigned c = click_current_cpu_id();
	_portTable[c].put(addr, port);
}

inline bool
TCPInfo::pid_valid(int pid)
{
	return (pid >= 0 && pid < MAX_PIDS);
}

inline bool
TCPInfo::sock_valid(int sockfd)
{
	return (sockfd >= 3 && sockfd < int(_usr_capacity));
}

inline int 
TCPInfo::sock_get(int pid, TCPState *s)
{
	unsigned c = click_current_cpu_id();

	// Check if all descriptors are used
	if (_sockFDesc[c][pid].size() == 0)
		return -1;

	int sockfd = _sockFDesc[c][pid].front();
	_sockFDesc[c][pid].pop_front();
	_sockTable[c][pid][sockfd] = s;

	return sockfd;
}

inline void
TCPInfo::sock_put(int pid, int sockfd)
{
	unsigned c = click_current_cpu_id();

	_sockTable[c][pid][sockfd] = NULL;
	_sockFDesc[c][pid].push_back(sockfd);
}

inline TCPState *
TCPInfo::sock_lookup(int pid, int sockfd)
{
	if (!sock_valid(sockfd))
		return NULL;

	unsigned c = click_current_cpu_id();
	return _sockTable[c][pid][sockfd];
}

# if HAVE_ALLOW_EPOLL
inline int
TCPInfo::epoll_fd_get(int pid)
{
	unsigned c = click_current_cpu_id();
	if (_epollFDesc[c][pid].size() == 0)
		return -1;

	int epfd = _epollFDesc[c][pid].front();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	
	if (eq == NULL)
		_epollTable[c][pid][epfd] = new TCPEventQueue();
	else
		return -1;

	_epollFDesc[c][pid].pop_front();
	return epfd;
}

inline bool
TCPInfo::epoll_fd_valid(int epfd)
{
	return (epfd > 0 && epfd < MAX_EPOLLFD);
}


inline bool
TCPInfo::epoll_fd_exists(int pid, int epfd)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	if (eq)
		return true;

	return false;
}

inline void
TCPInfo::epoll_fd_put(int pid, int epfd)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	delete(eq);
	_epollFDesc[c][pid].push_back(epfd);
}

inline int
TCPInfo::epoll_eq_size(int pid, int epfd)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	return eq->size();
}

inline TCPEventQueue::iterator
TCPInfo::epoll_eq_begin(int pid, int epfd)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	if (eq->size() > 0) {
		return eq->begin();
	}
	return eq->end();
}

inline TCPEventQueue::iterator
TCPInfo::epoll_eq_end(int pid, int epfd)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	return eq->end();
}

inline void
TCPInfo::epoll_eq_erase(int pid, int epfd, TCPEvent* ev)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
	eq->erase(ev);
	return;
}

inline void
TCPInfo::epoll_eq_insert(int pid, int epfd, TCPEvent* tev)
{
	unsigned c = click_current_cpu_id();
	TCPEventQueue *eq = _epollTable[c][pid][epfd];
// 	TCPEvent *ev = eq->allocate();
// 	new(reinterpret_cast<void *>(ev)) TCPEvent(tev);
	eq->push_back(tev);
}

// inline TCPEvent*
// TCPInfo::epoll_eq_pop_front(int pid, int epfd)
// {
// 	unsigned c = click_current_cpu_id();
// 	TCPEventQueue *eq = _epollTable[c][pid][epfd];
// 	TCPEvent* f = NULL;
// 	if (eq->size() > 0) {
// 		f = eq->front();
// 		eq->pop_front();
// // 		eq->deallocate(f);
// 	}
// 	return f;
// }
# endif // HAVE_ALLOW_EPOLL

CLICK_ENDDECLS
#endif
