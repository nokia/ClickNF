/*
 * tcpsocket.{cc,hh} -- TCP socket API
 * Rafael Laufer, Massimo Gallo, Myriana Rifai
 *
 * Copyright (c) 2018 Nokia Bell Labs
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
#include <click/error.hh>
#include <click/router.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/tcp.hh>
#include <linux/tcp.h>
#include "tcptimers.hh"
#include "tcpstate.hh"
#include "tcpinfo.hh"
#include "tcplist.hh"
#include "tcpackoptionsencap.hh"
#include "util.hh"
CLICK_DECLS

TCPSocket *TCPSocket::_socket = NULL;
#if HAVE_DPDK
static uint8_t key_be[RSS_HASH_KEY_LENGTH];
#endif // HAVE_DPDK

TCPSocket::TCPSocket()
{
}

int
TCPSocket::configure(Vector<String> &, ErrorHandler *errh)
{
	if (_socket)
		return errh->error("TCPSocket can only be configured once");

	_socket = this;

	_nthreads = master()->nthreads();
#if HAVE_DPDK
	rte_convert_rss_key((uint32_t *)DPDK::key, (uint32_t *)key_be, RSS_HASH_KEY_LENGTH); 
#endif

	return 0;
}

int
TCPSocket::socket(int pid, int domain, int type, int protocol)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Only SOCK_NONBLOCK is accepted as flags for now
	int flags = (type & SOCK_NONBLOCK);
	type = (type & ~SOCK_NONBLOCK);

	// Check parameters
	if (unlikely(domain != AF_INET || type != SOCK_STREAM || protocol != 0)) {
		errno = EINVAL;
		return -1;
	}

	// Check system capacity 
	if (unlikely(TCPInfo::sys_sockets() == TCPInfo::sys_capacity())) {
		errno = ENFILE;
		return -1;
	}

	// Check user capacity
	if (unlikely(TCPInfo::usr_sockets(pid) == TCPInfo::usr_capacity())) {
		errno = EMFILE;
		return -1;
	}

	// Allocate TCB
	TCPState *s = TCPState::allocate();
	if (unlikely(!s)) {
		errno = ENOMEM;
		return -1;
	}

	// Initialize TCB
	new(reinterpret_cast<void *>(s)) TCPState(IPFlowID());
	s->pid = pid;
	s->flags = flags;
	s->sockfd = -1;
	s->task = current;


	// Get socket file descriptor
	s->sockfd = TCPInfo::sock_get(pid, s);
	click_assert(s->sockfd > 0);

	// Increment socket counters
	TCPInfo::inc_sys_sockets();
	TCPInfo::inc_usr_sockets(pid);

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return s->sockfd;
}

int
TCPSocket::fcntl(int pid, int sockfd, int cmd)
{
	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}
	
	switch (cmd) {
		case F_GETFL:
			return s->flags; 
			break;
	}
	return 0;
  
}

int
TCPSocket::set_task(int pid, int sockfd, BlockingTask * t)
{
	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}
	
	//Override
	s->task = t;
	return 0;
}

int
TCPSocket::fcntl(int pid, int sockfd, int cmd, int arg)
{
	
	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}
	
	switch (cmd) {
		case F_SETFL:
			s->flags = arg;
			return 1;
			break;
	}
	return 0;
}

int
TCPSocket::setsockopt(int pid, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	if( level == SOL_SOCKET){
		switch (optname) {
		case SO_LINGER:
			struct linger *ling;
			if (optlen < sizeof(struct linger)) {
				errno = EINVAL;
				return -1;
			}

			ling = (struct linger*) optval;
			if (ling->l_linger != 0) {
				errno = EOPNOTSUPP; 
				return -1;
			}

			if (!ling->l_onoff)
				s->flags &= ~SOCK_LINGER; //TODO XOR
			else { 
				s->flags |= SOCK_LINGER; 
			}
			
			break;

		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}
	else if ( level == SOL_TCP ){	  
		switch (optname) {
		case TCP_MAXSEG:
			uint16_t *snd_mss;
			if (optlen < sizeof(uint16_t)) {
				errno = EINVAL;
				return -1;
			}

			snd_mss = (uint16_t*) optval;
			s->snd_mss = MIN(*snd_mss, TCP_SND_MSS_MAX);
			break;

		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}
	else if ( level == SOL_IP ){
		switch (optname) {
		case IP_BIND_ADDRESS_NO_PORT:
			s->bind_address_no_port = true;
			break;

		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}

#if CLICK_STATS >= 2
	 delta = click_get_cycles() - start_cycles;
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

int
TCPSocket::getsockopt(int pid, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	if( level == SOL_SOCKET){
		switch (optname) {
		case SO_LINGER:
			struct linger *ling;
			if (optlen < sizeof(struct linger)) {
				errno = EINVAL;
				return -1;
			}

			ling = (struct linger*) optval;
			if (ling->l_linger != 0) {
				errno = EOPNOTSUPP; 
				return -1;
			}

			if (s->flags & SOCK_LINGER)
				ling->l_onoff = SOCK_LINGER;
			else
				ling->l_onoff = 0;
			break;

		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}
	else if (level == SOL_TCP){
		switch (optname) {
		
		case TCP_MAXSEG:
			uint16_t *snd_mss;
			if (optlen < sizeof(uint16_t)) {
				errno = EINVAL;
				return -1;
			}

			snd_mss = (uint16_t*) optval;
			*snd_mss = s->snd_mss;
			break;
			
		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}

#if CLICK_STATS >= 2
	 delta = click_get_cycles() - start_cycles;
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

#if HAVE_DPDK
//NOTE For RSS operations we assume symmetric key and TCP + IP hash

uint32_t
TCPSocket::rss_hash(IPFlowID flow)
{
	// Values are stored in host byte order
	union rte_thash_tuple tuple;

	// Insert all fields
	tuple.v4.src_addr = ntohl(flow.saddr().addr());
	tuple.v4.dst_addr = ntohl(flow.daddr().addr());
	tuple.v4.sport = ntohs(flow.sport());
	tuple.v4.dport = ntohs(flow.dport());

	// Compute RSS hash
	return rte_softrss_be((uint32_t *)&tuple, 3, (uint8_t *)key_be);
}

int
TCPSocket::rss_sport(IPFlowID flow)
{
	// Values are stored in host byte order
	union rte_thash_tuple tuple;

	// Insert all fields, except source port
	tuple.v4.src_addr = ntohl(flow.saddr().addr());
	tuple.v4.dst_addr = ntohl(flow.daddr().addr());
	tuple.v4.sport = 0;
	tuple.v4.dport = ntohs(flow.dport());

	// Compute hash of source and destination IP addresses
	uint32_t h1 = rte_softrss_be((uint32_t *)&tuple, 2, (uint8_t *)key_be);

	// Pointer to ports
	uint32_t *tuple_ports = ((uint32_t *)&tuple) + 2;

	int c = -1;
	int id = (int)click_current_cpu_id();
	uint16_t port = 0;
	uint16_t start = (uint16_t)click_random(1024, 65535);
	uint16_t p = start;
	do {
		// Fill flow tuple
		tuple.v4.sport = p;

		// Compute hash of source and destination ports
		uint32_t h2 = rte_softrss_be(tuple_ports, 1, (uint8_t *)&key_be[8]);

		// Compute final hash
		uint32_t hash = (h1 ^ h2) & 127;

		// Compute core as (hash % nthreads)
		c = mod(hash, _socket->_nthreads);

		// If an available port maps to our core, get out
		if (c == id && TCPInfo::port_lookup(flow.saddr(), p)) {
			port = p;
			break;
		}

		// Otherwise, increase port number
		if (unlikely(p == 65535))
			p = 1024;
		else
			p++;

		// DPDK doc says that uses the LSB of the hash to access indirection 
		// table (i.e., 128 queues, use 7 least significant bits)
		// default is: 4 queues 
		//     -> h=0 q=0, h=1 q=1, h=2 q=2, h=3 q=3, h=4 q=0, h=5 q=1 ...
	} while (p != start);

	if (port == 0) {
		errno = EADDRINUSE;
		return -1;
	}

	return port;
}
#endif // HAVE_DPDK

int
TCPSocket::bind(int pid, int sockfd, IPAddress &addr, uint16_t &port)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	// Address/port binding
	int r = __bind(s, addr, port, s->bind_address_no_port);

#if CLICK_STATS >= 2
	delta = click_get_cycles() - start_cycles;
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return r;
}

int
TCPSocket::__bind(TCPState *s, IPAddress &addr, uint16_t &port, bool bind_address_no_port)
{
	// Check if socket is already bound to an address/port
	if (unlikely(s->bound())) {
		errno = EINVAL;
		return -1;
	}
	click_assert(s->flow.saddr().empty() && s->flow.sport() == 0);

	//Allow any address to be added. Do not check fo the address
	
// 	// Only accept requests for our IP addresses or 0.0.0.0
// 	Vector<IPAddress> my_addrs = TCPInfo::addr();
// 	if (unlikely(addr.empty()))
// 		addr = my_addrs[0];
// 	else if (find(my_addrs.begin(), my_addrs.end(), addr) == my_addrs.end()) {
// 		errno = EADDRNOTAVAIL;
// 		return -1;
// 	}
	


	// If port is not specified (and bind_address_no_port, try to bind to an ephemeral port
	if (port != 0){
		if (!TCPInfo::port_get(addr, port, s)) {
			      errno = EADDRINUSE;
			      return -1;
		      }
	}
	else if (!bind_address_no_port) {
		// Select a random source port
		uint16_t start = (uint16_t)click_random(1024, 65535);
		uint16_t p = start;

		// Try to lock this port and, if already taken, try the next one
		do {
			if (TCPInfo::port_get(addr, p, s)) {
				port = p;
				break;
			}

			if (unlikely(p == 65535))
				p = 1024;
			else
				p++;
		} while (p != start);

		// No ports are available
		if (port == 0) {
			errno = EADDRINUSE;
			return -1;
		}
	}

	// Set IP address and port
	s->flow.set_saddr(addr);
	s->flow.set_sport(htons(port));

	return 0;
}

int
TCPSocket::listen(int pid, int sockfd, int backlog)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists of invalid backlog
	if (unlikely(!TCPInfo::pid_valid(pid) || backlog <= 0)) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (s->error) {
		errno = s->error;
		return -1;
	}

	// Check socket state and if it is not bound to an address/port
	if (unlikely(s->state != TCP_CLOSED || !s->bound())) {
		errno = EADDRINUSE;
		return -1;
	}

	// Update TCP state
	s->flow.set_daddr(IPAddress());
	s->flow.set_dport(0);
	s->backlog = backlog;
	s->state = TCP_LISTEN;

	// Insert flow in the table
	int r = TCPInfo::flow_insert(s);
	click_assert(r == 0),(void)r;

#if CLICK_STATS >= 2
	delta = click_get_cycles() - start_cycles;
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

int
TCPSocket::accept(int pid, int sockfd, IPAddress &addr, uint16_t &port)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	// Make sure socket is listening
	if (unlikely(s->state != TCP_LISTEN)) {
		errno = EINVAL;
		return -1;
	}
#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
#endif
	// Wait for nonempty accept queue
	int ret = s->wait_event(TCP_WAIT_ACQ_NONEMPTY);
#if CLICK_STATS >= 2
	start_cycles = click_get_cycles();
#endif
	if (ret) {
		errno = ret;
		return -1;
	}
	click_assert(!s->acq_empty());

	// Check system capacity 
	if (unlikely(TCPInfo::sys_sockets() == TCPInfo::sys_capacity())) {
		errno = ENFILE;
		return -1;
	}

	// Check user capacity
	if (unlikely(TCPInfo::usr_sockets(pid) == TCPInfo::usr_capacity())) {
		errno = EMFILE;
		return -1;
	}

	// Get the state of the new flow
	TCPState *t = s->acq_front();
	click_assert(t);
	s->acq_pop_front();

	if(s->acq_empty() && s->event && s->epfd){
		s->event->event &= ~(TCP_WAIT_ACQ_NONEMPTY);		
		if (s->event->event == 0){
			TCPInfo::epoll_eq_erase(pid,s->epfd, s->event);
			delete(s->event);
			s->event = NULL;
		}
	}
	
	// If closed, remove it from the flow table and deallocate
	if (unlikely(t->state == TCP_CLOSED)) {
		TCPInfo::flow_remove(t);
		TCPState::deallocate(t);
		errno = ECONNABORTED;
		return -1;
	}

	// Otherwise add it to the socket table with available sockfd
	t->sockfd = TCPInfo::sock_get(pid, t);
	click_assert(t->sockfd > 0);

	// Increment socket counters
	TCPInfo::inc_sys_sockets();
	TCPInfo::inc_usr_sockets(pid);

	// Fill in the client's address and port
	addr = t->flow.daddr();
	port = ntohs(t->flow.dport());
	
#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return t->sockfd;
}

int
TCPSocket::connect(int pid, int sockfd, IPAddress daddr, uint16_t dport)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;
	int ret = 0;

	// Check parameters
	if (unlikely(daddr.empty() || dport == 0)) {
		errno = EINVAL;
		return -1;
	}

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	// Check the connection state
	if (unlikely(s->state != TCP_CLOSED || s->error != 0)) {
		if (s->flags & SOCK_NONBLOCK)
			errno = EALREADY;
		else
			errno = EISCONN;

		return -1;
	}

	// Bind to a local interface and port, if needed
	if(!s->bound()){
		IPFlowID flow;
		IPAddress saddr(0);
		uint16_t sport = 0;
		if (s->flow.saddr().empty()) {
			click_assert(s->flow.sport() == 0);
			Vector<IPAddress> my_addrs = TCPInfo::addr();
			saddr = my_addrs[0];
		}
		else
			saddr = s->flow.saddr();
	#if HAVE_DPDK
		flow.assign(saddr, htons(sport), daddr, htons(dport));
		ret = rss_sport(flow);
		if (ret == -1)
			return -1; // errno is set by rss_sport()
		sport = (uint16_t)ret;
	#endif // HAVE_DPDK

		//Bind asking for rnd port if sport = 0
		ret = __bind(s, saddr, sport, false);
		if (ret) {
			return -1; // errno is set by bind()
		}
	}
	// Complete flow tuple
	IPFlowID f = s->flow;
	f.set_daddr(daddr);
	f.set_dport(htons(dport));

	// Initialize TCB
	s->state      = TCP_SYN_SENT;
	s->flow       = f;
	s->snd_isn    = click_random(0, 0xFFFFFFFFU);
	s->snd_una    = s->snd_isn;
	s->snd_nxt    = s->snd_isn + 1;
	s->is_passive = false;

	// Reset retranstission timeout
	s->snd_rto = TCP_RTO_INIT;
	  
	// Initialize timers
	unsigned c = click_current_cpu_id();
	s->rtx_timer.assign(TCPTimers::rtx_timer_hook, s);
	s->rtx_timer.initialize(TCPTimers::element(), c);

	if (TCPInfo::cong_control() == 2){
		s->tx_timer.assign(TCPTimers::tx_timer_hook, s);
		s->tx_timer.initialize(TCPTimers::element(), c);
	}
#if HAVE_TCP_KEEPALIVE
	s->keepalive_timer.assign(TCPTimers::keepalive_timer_hook, s);
	s->keepalive_timer.initialize(TCPTimers::element(), c);
#endif

#if HAVE_TCP_DELAYED_ACK
	s->delayed_ack_timer.assign(TCPTimers::delayed_ack_timer_hook, s);
	s->delayed_ack_timer.initialize(TCPTimers::element(), c);
#endif

	// Insert flow into the table
	ret = TCPInfo::flow_insert(s);
	click_assert(ret == 0);

	// Create SYN packet and send it
	WritablePacket *q = Packet::make(TCP_HEADROOM, NULL, 0, 0);
	click_assert(q);
	SET_TCP_STATE_ANNO(q, (uint64_t)s);
#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
#endif
	_socket->output(TCP_SOCKET_OUT_SYN_PORT).push(q);
#if CLICK_STATS >= 2
	start_cycles = click_get_cycles();
#endif

	// If the socket is nonblocking, the connection 
	// status will be known later using poll()/select()
	if (s->flags & SOCK_NONBLOCK) {
		errno = EINPROGRESS;
		return -1;
	}
#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
#endif
	// Otherwise, we block and wait for a connection established or an error
	ret = s->wait_event(TCP_WAIT_CON_ESTABLISHED);
#if CLICK_STATS >= 2
	start_cycles = click_get_cycles();
#endif

	// If there is an error when we are back, erase the socket and notify user
	if (ret) {
		// Remove from port table
		TCPInfo::port_put(s->flow.saddr(), ntohs(s->flow.sport()));

		// Remove from socket table
		TCPInfo::sock_put(pid, sockfd);

		// Decrement socket counters
		TCPInfo::dec_sys_sockets();
		TCPInfo::dec_usr_sockets(pid);

		// Remove from flow table
		TCPInfo::flow_remove(s);

		// Wait for a grace period and deallocate TCB
		TCPState::deallocate(s);

		errno = ret;
		return -1;
	}

	click_assert(s->state == TCP_ESTABLISHED);

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

int
TCPSocket::send(int pid, int sockfd, const char *buffer, size_t length)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;
	int ret = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	switch (s->state) {
	case TCP_CLOSED:
		// "If the user does not have access to such a connection, then return
		//  "error:  connection illegal for this process".
		//
		//  Otherwise, return "error:  connection does not exist"."
		errno = ENOTCONN;
		return -1;

	case TCP_LISTEN:
		// "If the foreign socket is specified, then change the connection
		//  from passive to active, select an ISS.  Send a SYN segment, set
		//  SND.UNA to ISS, SND.NXT to ISS+1.  Enter SYN-SENT state.  Data
		//  associated with SEND may be sent with SYN segment or queued for
		//  transmission after entering ESTABLISHED state.  The urgent bit if
		//  requested in the command must be sent with the data segments sent
		//  as a result of this command.  If there is no room to queue the
		//  request, respond with "error:  insufficient resources".  If
		//  Foreign socket was not specified, then return "error:  foreign
		//  socket unspecified"."
		//
		// Changing from passive to active is not allowed, so return an error
		errno = ENOTCONN;
		return -1;

	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		// "Queue the data for transmission after entering ESTABLISHED state.
		//  If no space to queue, respond with "error:  insufficient
		//  resources".
		if (s->flags & SOCK_NONBLOCK)
			errno = EINPROGRESS;
		else
			click_assert(0);  // Should never happen 

		return -1;

	case TCP_ESTABLISHED:
	case TCP_CLOSE_WAIT: {
		// "Segmentize the buffer and send it with a piggybacked
		//  acknowledgment (acknowledgment value = RCV.NXT).  If there is
		//  insufficient space to remember this buffer, simply return "error:
		//  insufficient resources".
		//
		//  If the urgent flag is set, then SND.UP <- SND.NXT-1 and set the
		//  urgent pointer in the outgoing segments."

		// If too much data, limit how much we can get
		length = MIN(length, TCPInfo::wmem() >> 1);

		// Check if there is enough space left for the message
#if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
#endif
		ret = s->wait_event(TCP_WAIT_TXQ_HALF_EMPTY);
#if CLICK_STATS >= 2
		start_cycles = click_get_cycles();
#endif
		if (ret) {
			errno = ret;
			return -1;
		}
		click_assert(s->txq.bytes() + length <= TCPInfo::wmem());

		// We allow zero-length and null-buffer send() calls for nonblocking 
		// sockets to know if there is enough space in the TX queue w/o poll()
		if (buffer && length > 0) {
			// Effective MSS when TCP options have maximum length
			uint16_t mss = s->snd_mss - TCPAckOptionsEncap::min_oplen(s);

			// Segmentation
			for (uint32_t offset = 0; offset < length; offset += mss) {
				const char *data = buffer + offset;
				uint32_t len = MIN(mss, length - offset);

				// Create the packet
				WritablePacket *p = Packet::make(TCP_HEADROOM, data, len, 0);
				if (!p) {
					errno = ENOMEM;
					return -1;
				}

				// Insert it into the TX queue
				s->txq.push_back(p);
			}

			// Send an empty packet to trigger a potential transmission
			// Do not clear the annotations, as packet will be killed
			WritablePacket *q = 
			             Packet::make(TCP_HEADROOM, NULL, 0, s->snd_mss, false);
			SET_TCP_STATE_ANNO(q, (uint64_t)s);
#if CLICK_STATS >= 2
			delta += (click_get_cycles() - start_cycles);
#endif
			_socket->output(TCP_SOCKET_OUT_TXT_PORT).push(q);
#if CLICK_STATS >= 2
			start_cycles = click_get_cycles();
#endif
		}

		if( (s->txq.bytes() >= TCPInfo::wmem()) && s->event && s->epfd){
			s->event->event &= ~(TCP_WAIT_TXQ_HALF_EMPTY);
			if (s->event->event == 0){
				TCPInfo::epoll_eq_erase(pid,s->epfd, s->event);
				delete(s->event);
				s->event = NULL;
			}
		}
		return length;
	}

	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
	case TCP_CLOSING:
	case TCP_TIME_WAIT:
	case TCP_LAST_ACK:
		// "Return "error:  connection closing" and do not service request."
	default:
		errno = EPIPE;
		return -1;
	}

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

int
TCPSocket::push(int pid, int sockfd, Packet *p)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Make sure we are running in user context
	click_assert(current);

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Lock state

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	// Make sure push() is only allowed in certain states
	if (unlikely(s->state != TCP_ESTABLISHED && s->state != TCP_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}

	// Effective MSS when TCP options have maximum length
	uint16_t mss = s->snd_mss - TCPAckOptionsEncap::min_oplen(s);

	//Allow push without packet to check TXQ space
	if (!p)
	  return (TCPInfo::wmem()-s->txq.bytes());
	
	int length = 0;
	
	// Make sure packets are not too big
	for (Packet *q = p; q; q = q->next()) {
		length += q->length();
		if (q->length() > mss) {
			errno = EMSGSIZE;
			return -1;
		}
	}

	if (TCPInfo::cong_control() == 2)
		s->rs->rate_check_app_limited(s);

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
#endif
	// Check if there is enough space left for the message
	int ret = s->wait_event(TCP_WAIT_TXQ_HALF_EMPTY);
#if CLICK_STATS >= 2
	start_cycles = click_get_cycles();
#endif
	if (ret) {
		errno = ret;
		return -1;
	}

	// Insert packets into the TX queue
	do {
		Packet *q = p->next();
 		if (p->timestamp_anno() > 0)
 			p->set_timestamp_anno(Timestamp());
		s->txq.push_back(p);
		p = q;
	} while (p);

	// Send an empty packet to trigger a potential transmission. 
	// Do not clear the annotations, as packet will be killed
	WritablePacket *q = Packet::make(TCP_HEADROOM, NULL, 0, s->snd_mss, false); 
	SET_TCP_STATE_ANNO(q, (uint64_t)s);
#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
#endif
	_socket->output(TCP_SOCKET_OUT_TXT_PORT).push(q);
#if CLICK_STATS >= 2
	start_cycles = click_get_cycles();
#endif

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	if( (s->txq.bytes() >= TCPInfo::wmem()) && s->event && s->epfd){
		s->event->event &= ~(TCP_WAIT_TXQ_HALF_EMPTY);
		if (s->event->event == 0){
			TCPInfo::epoll_eq_erase(pid,s->epfd, s->event);
			delete(s->event);
			s->event = NULL;
		}
	}
	return length;
}

int
TCPSocket::recv(int pid, int sockfd, char *buffer, size_t length)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists or bad length
	if (unlikely(!TCPInfo::pid_valid(pid) || length <= 0)) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	switch (s->state) {
	case TCP_CLOSED:
		errno = ENOTCONN;
		return -1;

	case TCP_LISTEN:
		// "If the user does not have access to such a connection, return
		//  "error:  connection illegal for this process".
		//
		//  Otherwise return "error:  connection does not exist"."
		errno = ENOTCONN;
		return -1;

	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		// "Queue for processing after entering ESTABLISHED state.  If there
		//  is no room to queue this request, respond with "error:
		//  insufficient resources"."
		if (s->flags & SOCK_NONBLOCK)
			errno = EINPROGRESS;
		else
			click_assert(0);  // Should never happen 

		return -1;

	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2: {
		// "If insufficient incoming segments are queued to satisfy the
		//  request, queue the request.  If there is no queue space to
		//  remember the RECEIVE, respond with "error:  insufficient
		//  resources".
		//
		//  Reassemble queued incoming segments into receive buffer and return
		//  to user.  Mark "push seen" (PUSH) if this is the case.
		//
		//  If RCV.UP is in advance of the data currently being passed to the
		//  user notify the user of the presence of urgent data.
		//
		//  When the TCP takes responsibility for delivering data to the user
		//  that fact must be communicated to the sender via an
		//  acknowledgment.  The formation of such an acknowledgment is
		//  described below in the discussion of processing an incoming
		//  segment."

#if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
#endif
		// Block until we receive something or change state
		int ret = s->wait_event(TCP_WAIT_RXQ_NONEMPTY | TCP_WAIT_FIN_RECEIVED);
#if CLICK_STATS >= 2
		start_cycles = click_get_cycles();
#endif
		if (ret) {
			errno = ret;
			return -1;
		}
		click_assert(!s->rxq.empty() || s->state == TCP_CLOSING    ||
		                                s->state == TCP_TIME_WAIT  ||
		                                s->state == TCP_CLOSE_WAIT ||
		                                s->state == TCP_LAST_ACK);
	}
		// fallthrough
	case TCP_CLOSE_WAIT: {
		// "Since the remote side has already sent FIN, RECEIVEs must be
		//  satisfied by text already on hand, but not yet delivered to the
		//  user.  If no text is awaiting delivery, the RECEIVE will get a
		//  "error:  connection closing" response.  Otherwise, any remaining
		//  text can be used to satisfy the RECEIVE."
		int l = 0;
		while (length && !s->rxq.empty()) {
			Packet *p = s->rxq.front();
			click_assert(p);

			size_t len = MIN(p->length(), length);
			memcpy(buffer, p->data(), len);

			if (len == p->length()) {
				s->rxq.pop_front();
				p->kill();
			}
			else
				s->rxq.pull_front(len);

			// Increase receive window
			s->rcv_wnd += len;

			buffer += len;
			length -= len;
			l += len;
		}

		if(s->rxq.empty() && s->event && s->epfd){
			s->event->event &= ~(TCP_WAIT_RXQ_NONEMPTY);
			if (s->event->event == 0){
				TCPInfo::epoll_eq_erase(pid,s->epfd, s->event);
				delete(s->event);
				s->event = NULL;
			}
		}
			
		return l;
	}

	case TCP_CLOSING:
	case TCP_TIME_WAIT:
	case TCP_LAST_ACK:
	default:
		errno = EPIPE;
		return -1;
	}

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

Packet *
TCPSocket::pull(int pid, int sockfd, int npkts)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;
	Packet* p = NULL;

	// Check if pid exists or bad npkts
	if (unlikely(!TCPInfo::pid_valid(pid) || npkts <= 0)) {
		errno = EINVAL;
		return NULL;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return NULL;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return NULL;
	}

	switch (s->state) {
	case TCP_CLOSED:
		errno = ENOTCONN;
		return NULL;

	case TCP_LISTEN:
		// "If the user does not have access to such a connection, return
		//  "error:  connection illegal for this process".
		//
		//  Otherwise return "error:  connection does not exist"."
		errno = ENOTCONN;
		return NULL;

	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		// "Queue for processing after entering ESTABLISHED state.  If there
		//  is no room to queue this request, respond with "error:
		//  insufficient resources"."
		if (s->flags & SOCK_NONBLOCK)
			errno = EINPROGRESS;
		else
			click_assert(0);  // Should never happen 

		return NULL;

	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2: {
		// "If insufficient incoming segments are queued to satisfy the
		//  request, queue the request.  If there is no queue space to
		//  remember the RECEIVE, respond with "error:  insufficient
		//  resources".
		//
		//  Reassemble queued incoming segments into receive buffer and return
		//  to user.  Mark "push seen" (PUSH) if this is the case.
		//
		//  If RCV.UP is in advance of the data currently being passed to the
		//  user notify the user of the presence of urgent data.
		//
		//  When the TCP takes responsibility for delivering data to the user
		//  that fact must be communicated to the sender via an
		//  acknowledgment.  The formation of such an acknowledgment is
		//  described below in the discussion of processing an incoming
		//  segment."

#if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
#endif
		// Block until we receive something or change state
		int ret = s->wait_event(TCP_WAIT_RXQ_NONEMPTY | TCP_WAIT_FIN_RECEIVED);
#if CLICK_STATS >= 2
		start_cycles = click_get_cycles();
#endif
		if (ret) {
			errno = ret;
			return NULL;
		}
	}
		// fallthrough
	case TCP_CLOSE_WAIT: {
		// "Since the remote side has already sent FIN, RECEIVEs must be
		//  satisfied by text already on hand, but not yet delivered to the
		//  user.  If no text is awaiting delivery, the RECEIVE will get a
		//  "error:  connection closing" response.  Otherwise, any remaining
		//  text can be used to satisfy the RECEIVE."
		p = NULL;
		if (s->rxq.empty()) 
			p = Packet::make((const void *)NULL, 0);
		else {
			Packet *t = NULL;
			while (Packet *q = s->rxq.front()) {
				s->rxq.pop_front();
				SET_TCP_STATE_ANNO(q, 0);

				// Increase receive window
				s->rcv_wnd += q->length();

				// Set head and tail
				if (!p)
					p = t = q;
				else {
					// Update tail
					q->set_next(NULL);
					t->set_next(q);
					t = q;
				}

				if (--npkts == 0)
					break;
			}
		}

		if(s->rxq.empty() && s->event && s->epfd){
			s->event->event &= ~(TCP_WAIT_RXQ_NONEMPTY);
			if (s->event->event == 0){
				TCPInfo::epoll_eq_erase(pid,s->epfd, s->event);
				delete(s->event);
				s->event = NULL;
			}
		}
		
		return p;
	}

	case TCP_CLOSING:
	case TCP_TIME_WAIT:
	case TCP_LAST_ACK:
	default:
		errno = EPIPE;
		return NULL;
	}

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return NULL;
}

int
TCPSocket::fsync(int pid, int sockfd)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Check for pending errors
	if (unlikely(s->error)) {
		errno = s->error;
		return -1;
	}

	switch (s->state) {
	case TCP_CLOSED:
		errno = ENOTCONN;
		return -1;

	case TCP_LISTEN:
		errno = ENOTCONN;
		return -1;

	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		if (s->flags & SOCK_NONBLOCK)
			errno = EINPROGRESS;
		else
			click_assert(0);  // Should never happen 

		return -1;

	case TCP_ESTABLISHED:
	case TCP_CLOSE_WAIT: {
#if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
#endif
		// Wait until RTX queue is empty
		int ret = s->wait_event(TCP_WAIT_RTXQ_EMPTY);
#if CLICK_STATS >= 2
		start_cycles = click_get_cycles();
#endif
		if (ret) {
			errno = ret;
			return -1;
		}
		click_assert(s->rtxq.empty());

		break;
	}

	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
	case TCP_CLOSING:
	case TCP_TIME_WAIT:
	case TCP_LAST_ACK:
	default:
		errno = EPIPE;
		return -1;
	}

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

int
TCPSocket::close(int pid, int sockfd)
{
#if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
#endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if sockfd exists
	if (unlikely(!s)) {
		errno = EBADF;
		return -1;
	}

	// Lock state
	switch (s->state) {
	case TCP_CLOSED:
		// "If the user does not have access to such a connection, return
		//  "error:  connection illegal for this process".
		//
		// Otherwise, return "error:  connection does not exist".
		TCPInfo::sock_put(pid, sockfd);

		TCPInfo::flow_remove(s);

		// Wait for a grace period and deallocate TCB
		TCPState::deallocate(s);

		// Decrement socket counters
		TCPInfo::dec_sys_sockets();
		TCPInfo::dec_usr_sockets(pid);

		break;

	case TCP_LISTEN: {
		// "Any outstanding RECEIVEs are returned with "error:  closing"
		//  responses.  Delete TCB, enter CLOSED state, and return."

		// Stop listening to new connections
		s->state = TCP_CLOSED;

		// Clear descriptors in the accept queue but not yet accept()'ed
		for (TCPState *t = s->acq_front(); t != s; t = t->acq_next) {
			click_assert(t);

			// Reset connection
			WritablePacket *p = Packet::make(TCP_HEADROOM, NULL, 0, 0);
			click_assert(p);
			SET_TCP_STATE_ANNO(p, (uint64_t)t);
#if CLICK_STATS >= 2
			delta += (click_get_cycles() - start_cycles);
#endif
			_socket->output(TCP_SOCKET_OUT_RST_PORT).push(p);
#if CLICK_STATS >= 2
			start_cycles = click_get_cycles() ;
#endif
			// Remove from flow table for now, deallocate it later
			TCPInfo::flow_remove(t);
		}

		// Remove from port table
		TCPInfo::port_put(s->flow.saddr(), ntohs(s->flow.sport()));

		// Remove from socket table
		TCPInfo::sock_put(pid, sockfd);

		// Decrement socket counters
		TCPInfo::dec_sys_sockets();
		TCPInfo::dec_usr_sockets(pid);

		// Remove from flow table
		TCPInfo::flow_remove(s);

		// Deallocate descriptors in accept queue
		for (TCPState *t = s->acq_front(); t != s; ) {
			TCPState *n = t->acq_next;
			TCPState::deallocate(t);
			t = n;
		}

		TCPState::deallocate(s);

		break;
	}

	case TCP_SYN_SENT: {
		// "Delete the TCB and return "error:  closing" responses to any
		//  queued SENDs, or RECEIVEs."

		// This should only be possible in nonblocking sockets
		click_assert(s->flags & SOCK_NONBLOCK);

		s->state = TCP_CLOSED;

		// Stop retransmission timer and flush RTX queue
		s->stop_timers();
		s->flush_queues();

		// Remove from port table
		TCPInfo::port_put(s->flow.saddr(), ntohs(s->flow.sport()));

		// Remove from socket table
		TCPInfo::sock_put(pid, sockfd);

		// Decrement socket counters
		TCPInfo::dec_sys_sockets();
		TCPInfo::dec_usr_sockets(pid);

		// Remove from flow table
		TCPInfo::flow_remove(s);

		// Wait for a grace period and deallocate TCB
		TCPState::deallocate(s);

		break;
	}

	case TCP_SYN_RECV:
		// "If no SENDs have been issued and there is no pending data to send,
		//  then form a FIN segment and send it, and enter FIN-WAIT-1 state;
		//  otherwise queue for processing after entering ESTABLISHED state."
		click_assert(0); // This should be impossible

	case TCP_ESTABLISHED: 
	case TCP_CLOSE_WAIT: {
		// RFC 793:
		//
		// "ESTABLISHED
		//
		//     Queue this until all preceding SENDs have been segmentized, then
		//     form a FIN segment and send it.  In any case, enter FIN-WAIT-1
		//     state.
		//
		//  CLOSE_WAIT
		//
		//     Queue this request until all preceding SENDs have been
		//     segmentized; then send a FIN segment, enter CLOSING state."
		//
		// RFC 1122:
		//
		// "(a)  CLOSE Call, CLOSE-WAIT state, p. 61: enter LAST-ACK
		//       state, not CLOSING."

#if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
#endif
		// If SOCK_LINGER not set (only SO_LINGER {on, 0} supported) wait until the TX queue is empty (or return EAGAIN if nonblocking)
		int ret = 0;
		if (!(s->flags & SOCK_LINGER)) {
		      ret = s->wait_event(TCP_WAIT_TXQ_EMPTY);
#if CLICK_STATS >= 2
		start_cycles = click_get_cycles() ;
#endif
		      if (ret) {
				  //Wait for txq 
				  if (s->flags & SOCK_NONBLOCK){
				      s->wait_event_reset();
				      s->wait_event_set(TCP_WAIT_TXQ_EMPTY);
				  }
				  errno = ret;
				  return -1;
		      }
		}
		
		if (s->state == TCP_ESTABLISHED)
			s->state = TCP_FIN_WAIT1;
		else
			s->state = TCP_LAST_ACK;

		WritablePacket *p = Packet::make(TCP_HEADROOM, NULL, 0, 0);
		click_assert(p);
		SET_TCP_STATE_ANNO(p, (uint64_t)s);

		if (s->flags & SOCK_LINGER) {
#if CLICK_STATS >= 2
			delta += (click_get_cycles() - start_cycles);
#endif
			// Send a RST packet
			_socket->output(TCP_SOCKET_OUT_RST_PORT).push(p);
#if CLICK_STATS >= 2
			start_cycles = click_get_cycles() ;
#endif
			// Stop retransmission timer and flush RTX queue
			s->stop_timers();
			s->flush_queues();

			// Get source address
			IPAddress saddr = s->flow.saddr();

			// Remove from port table
			if (!s->is_passive) {
				uint16_t port = ntohs(s->flow.sport());
				TCPInfo::port_put(saddr, port);
			}

			// Remove from flow table
			TCPInfo::flow_remove(s);

			// Wait for a grace period and deallocate TCB
			TCPState::deallocate(s);

			// Remove from socket table
			TCPInfo::sock_put(pid, sockfd);

			// Decrement socket counters
			TCPInfo::dec_sys_sockets();
			TCPInfo::dec_usr_sockets(pid);
		}
		else {
			click_assert(s->txq.empty());
			
			// Increase sequence number
			s->snd_nxt++;

#if CLICK_STATS >= 2
			delta += (click_get_cycles() - start_cycles);
#endif
			// Send a FIN packet
			_socket->output(TCP_SOCKET_OUT_FIN_PORT).push(p);
#if CLICK_STATS >= 2
			start_cycles = click_get_cycles() ;
#endif
		
		}

		//Cannot receive events on the socket anymore. Just waiting timeout o expire
		if (s->epfd)
			epoll_ctl(s->pid, s->epfd, EPOLL_CTL_DEL, sockfd, NULL);

		s->epfd = -1;

		break;
	  
	}

	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
		// "Strictly speaking, this is an error and should receive a "error:
		//  connection closing" response.  An "ok" response would be
		//  acceptable, too, as long as a second FIN is not emitted (the first
		//  FIN may be retransmitted though).
	case TCP_CLOSING:
	case TCP_TIME_WAIT:
	case TCP_LAST_ACK:
		// "Respond with "error:  connection closing"."
	default:
		errno = EPIPE;
		return -1;
	}

#if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
#endif
	return 0;
}

#if HAVE_ALLOW_POLL
int
TCPSocket::poll(int pid, struct pollfd *fds, size_t nfds, int timeout)
{
# if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
# endif
	errno = 0;
	int ret = 0;
	click_assert(current);

	// Check if nfds exceeds user capacity
	if (unlikely(!TCPInfo::pid_valid(pid) || nfds > TCPInfo::usr_capacity())) {
		errno = EINVAL;
		return -1;
	}

	do {
		for (size_t i = 0; i < nfds; i++) {
			int sockfd = fds[i].fd;

			// "The field fd contains a file descriptor for an open file. If
			//  this field is negative, then the corresponding events field is
			//  ignored and the revents field returns zero."
			if (sockfd < 0) {
				fds[i].revents = 0;
				continue;
			}

			// Get socket
			TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

			// Check if sockfd exists
			if (!s) {
				fds[i].revents = POLLNVAL;
				ret++;
				continue;
			}

			// Make sure socket is non-blocking
			if (!(s->flags & SOCK_NONBLOCK)) {
				fds[i].revents = POLLNVAL;
				ret++;
				continue;
			}

			// Reset returning mask
			fds[i].revents = 0;

			// Get which events we should wait for this socket descriptor
			bool in = (fds[i].events & POLLIN);
			bool out = (fds[i].events & POLLOUT);

			// Check socket state and set returning mask accordingly
			switch (s->state) {
			case TCP_CLOSED:
				fds[i].revents |= POLLHUP;
				ret++;
				break;

			case TCP_LISTEN:
				if (in) {
					if (s->wait_event_check(TCP_WAIT_ACQ_NONEMPTY)) {
						fds[i].revents |= POLLIN;
						ret++;
					}
					else
						s->wait_event_set(TCP_WAIT_ACQ_NONEMPTY);
				}
				break;

			case TCP_SYN_SENT:
			case TCP_SYN_RECV:
				if (out)
					s->wait_event_set(TCP_WAIT_CON_ESTABLISHED);
				break;

			case TCP_ESTABLISHED:
			case TCP_CLOSE_WAIT:
				if (in) {
					if (s->wait_event_check(TCP_WAIT_FIN_RECEIVED))
						fds[i].revents |= POLLIN;
					else
						s->wait_event_set(TCP_WAIT_FIN_RECEIVED);

					if (s->wait_event_check(TCP_WAIT_RXQ_NONEMPTY))
						fds[i].revents |= POLLIN;
					else
						s->wait_event_set(TCP_WAIT_RXQ_NONEMPTY);
				}

				if (out) {
					if (s->wait_event_check(TCP_WAIT_TXQ_HALF_EMPTY))
						fds[i].revents |= POLLOUT;
					else
						s->wait_event_set(TCP_WAIT_TXQ_HALF_EMPTY);
				}

				if (fds[i].revents)
					ret++;
				break;

			case TCP_FIN_WAIT1:
			case TCP_FIN_WAIT2:
			case TCP_CLOSING:
			case TCP_TIME_WAIT:
			case TCP_LAST_ACK:
			default:
				fds[i].revents |= POLLNVAL;
				ret++;
				break;
			}

			// If there is a pending error, notify it
			if (s->error) {
				if (fds[i].revents == 0)
					ret++;
				fds[i].revents |= POLLERR;
			}
		}

		// If there is at least one event or zero timeout, return
		if (ret > 0 || timeout == 0)
			break;

		// Unschedule task and yield the processor
		current->unschedule();

# if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
# endif
		if (timeout > 0) {
			Timestamp t = Timestamp::make_msec(timeout);
			current->yield_timeout(t, true);
			timeout -= t.msecval();
			click_assert(timeout >= 0);
		}
		else
			current->yield(true);
# if CLICK_STATS >= 2
		start_cycles = click_get_cycles() ;
# endif
	} while (timeout != 0);

	// Clear the wait flags
	for (size_t i = 0; i < nfds; i++) {
		int sockfd = fds[i].fd;
		TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

		if (!s)
			continue;

		s->wait_event_reset();
	}

# if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
# endif
	return ret;
}
#endif /* HAVE_ALLOW_POLL */

#if HAVE_ALLOW_EPOLL
int 
TCPSocket::epoll_create(int pid, int size)
{
# if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
# endif
	errno = 0;
	int epfd = -1;

	if (unlikely(size <= 0 || !TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	epfd = TCPInfo::epoll_fd_get(pid);
	if (unlikely(epfd < 0)) {
		errno = EMFILE;
		return -1;
	}

# if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
# endif
	return epfd;
}

int 
TCPSocket::epoll_ctl(int pid, int epfd, int op, int sockfd, struct epoll_event *event)
{
# if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
# endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Check if epfd is valid
	if (unlikely(!TCPInfo::epoll_fd_valid(epfd))) {
		errno = EBADF;
		return -1;
	}

	// Check if epfd exists
	if (unlikely(!TCPInfo::epoll_fd_exists(pid, epfd) || sockfd == epfd)) {
		errno = EINVAL;
		return -1;
	}

	// Get state
	TCPState *s = TCPInfo::sock_lookup(pid, sockfd);

	// Check if fd exists
	if (unlikely(!s || (event && sockfd != event->data.fd))) {
		errno = EBADF;
		return -1;
	}

	// Make sure socket is non-blocking
	if (unlikely(!(s->flags & SOCK_NONBLOCK))) {
		errno = EBADF;
		return -1;
	}

	switch (op) {
	case EPOLL_CTL_MOD:
		if (s->epfd != epfd) {
			errno = ENOENT;
			return -1;
		}

		if ((s->event != NULL) && (s->epfd>0)){
			TCPInfo::epoll_eq_erase(s->pid, s->epfd, s->event);
			delete(s->event);
			s->event = NULL;
		}
		
		s->epfd = -1;
		s->wait_event_reset();
		// fall through

	case EPOLL_CTL_ADD: {
		// Add sockfd with EPOLLEXCLUSIVE flag i.e., events
		// of this sockfd are delivered to a single epfd.
		if (s->epfd == epfd) {
			errno = EEXIST;
			return -1;
		}

		if (s->epfd > 0 || !event) {
			errno = EINVAL;
			return -1;
		}

		s->epfd = epfd;

		// Get which events we should wait for this socket descriptor
		bool in = (event->events & EPOLLIN);
		bool out = (event->events & EPOLLOUT);

		// Check for pending events
		// All input events
		if (in) {
			s->wait_event_set(TCP_WAIT_ACQ_NONEMPTY);
			s->wait_event_set(TCP_WAIT_RXQ_NONEMPTY);
			s->wait_event_set(TCP_WAIT_FIN_RECEIVED);
		}

		// All output events
		if (out) {
			s->wait_event_set(TCP_WAIT_TXQ_HALF_EMPTY);
			s->wait_event_set(TCP_WAIT_CON_ESTABLISHED);
		}

		//Manage events according to flow state
		TCPEvent* ev = NULL;
		switch (s->state) {
		case TCP_CLOSED: {
			ev = new TCPEvent(s, TCP_WAIT_CLOSED);
			TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
			s->event = ev;
			break;
		}
		case TCP_LISTEN:
			if (in && s->wait_event_check(TCP_WAIT_ACQ_NONEMPTY)) {
				for (int i = 0; i < s->acq_size; i++) {
					if (!s->event){
						ev = new TCPEvent(s, TCP_WAIT_ACQ_NONEMPTY);
						TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
						s->event = ev;
					}
					else
						s->event->event |= TCP_WAIT_ACQ_NONEMPTY;
				}
			}
			break;

		case TCP_SYN_SENT:
		case TCP_SYN_RECV:
			if (out && s->wait_event_check(TCP_WAIT_CON_ESTABLISHED)) {
				if (!s->event){
					ev = new TCPEvent(s, TCP_WAIT_CON_ESTABLISHED);
					TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
					s->event = ev;
				}
				else 
					s->event->event |= TCP_WAIT_CON_ESTABLISHED;
			}
			break;
				
		case TCP_ESTABLISHED:
		case TCP_CLOSE_WAIT:
			if (in && s->wait_event_check(TCP_WAIT_RXQ_NONEMPTY)) {
				if (!s->event){
					ev = new TCPEvent(s, TCP_WAIT_RXQ_NONEMPTY);
					TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
					s->event = ev;
				}
				else 
					s->event->event |= TCP_WAIT_RXQ_NONEMPTY;
				
			}

			if (in && s->wait_event_check(TCP_WAIT_FIN_RECEIVED)) {
				if (!s->event){
					ev = new TCPEvent(s, TCP_WAIT_FIN_RECEIVED);
					TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
					s->event = ev;
				}
				else 
					s->event->event |= TCP_WAIT_FIN_RECEIVED;
			}

			if (out && s->wait_event_check(TCP_WAIT_TXQ_HALF_EMPTY)) {
				if (!s->event){
					ev = new TCPEvent(s, TCP_WAIT_TXQ_HALF_EMPTY);
					TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
					s->event = ev;
				}
				else 
					s->event->event |= TCP_WAIT_TXQ_HALF_EMPTY;
			}
			
			//if an event has been added to the event queue and task exist, schedule blocked Blocking Task 
			if ((s->event) && (s->task) && (!s->task->scheduled()))
				s->task->reschedule();

			break;

		case TCP_FIN_WAIT1:
		case TCP_FIN_WAIT2:
		case TCP_CLOSING:
		case TCP_TIME_WAIT:
		case TCP_LAST_ACK:
		default:
			// Should never happen since socket is closed
			if (!s->event){
				ev = new TCPEvent(s, TCP_WAIT_ERROR);
				TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
				s->event = ev;
			}
			else 
				s->event->event |= TCP_WAIT_ERROR;
			break;
		}
			
		// If an an error occoured, save state in the event queue
		if (s->error){
		  	if (!s->event){
				ev = new TCPEvent(s, TCP_WAIT_ERROR);
				TCPInfo::epoll_eq_insert(pid, s->epfd, ev);
				s->event = ev;
			}
			else
				s->event->event |= TCP_WAIT_ERROR;
		}

		break;
	}

	case EPOLL_CTL_DEL:
		if (s->epfd != epfd) {
			errno = ENOENT;
			return -1;
		}

		if ((s->event != NULL) && (s->epfd>0)){
			TCPInfo::epoll_eq_erase(s->pid, s->epfd, s->event);
			delete(s->event);
			s->event = NULL;
		}
		
		s->epfd = -1;
		s->wait_event_reset();
		break;

	default:
		errno = EINVAL;
		return -1;
	}

# if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
# endif
	return 0;
}

int
TCPSocket::epoll_wait(int pid, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
# if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
# endif
	errno = 0;
	int ret = 0;
	click_assert(current);

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Check if epfd is valid
	if (unlikely(!TCPInfo::epoll_fd_valid(epfd))) {
		errno = EBADF;
		return -1;
	}

	// Check if epfd exists or non-positive maxevents
	if (unlikely(!TCPInfo::epoll_fd_exists(pid, epfd) || maxevents <= 0)) {
		errno = EINVAL;
		return -1;
	}

	do {
		// Let other tasks run. NOTE This is needed because epoll queue may remain full and never yeld.
		current->fast_reschedule();
		current->yield(true);

		// If there is at least one event or zero timeout, return
		if (TCPInfo::epoll_eq_size(pid, epfd) > 0 || timeout == 0)
			break;

		// Unschedule task and yield the processor
		current->unschedule();

# if CLICK_STATS >= 2
		delta += (click_get_cycles() - start_cycles);
# endif
		if (timeout > 0) {
			Timestamp t = Timestamp::make_msec(timeout);
			current->yield_timeout(t, true);
			timeout -= t.msecval();
			click_assert(timeout >= 0);
		}
		else
			current->yield(true);
# if CLICK_STATS >= 2
		start_cycles = click_get_cycles() ;
# endif					   
	} while (timeout != 0);

	
	//TODO Do not remove from event queue. It will be removed when the condition does not apply anymore (e.g., when executing recv, push, pull, send, etc. )... 
	TCPEventQueue::iterator it = TCPInfo::epoll_eq_begin(pid, epfd);
	TCPEventQueue::iterator e = TCPInfo::epoll_eq_end(pid, epfd);
	while ( it!=e && ret < maxevents) {
		TCPEvent* evnt = &(*it);
		TCPState *s = evnt->state;
		click_assert(s);

		// Check event and set returning mask accordingly
		events[ret].events = 0;
		int ev = evnt->event;
		while (ev) {
			// Check one event at a time
			int e = ((ev & (ev - 1)) == 0 ? ev : 1 << (ffs_lsb((unsigned)ev) - 1));
			switch (e) {
				case TCP_WAIT_CLOSED:
					events[ret].events |= EPOLLHUP;
					events[ret].data.fd = s->sockfd;
					break;

				case TCP_WAIT_FIN_RECEIVED:
				case TCP_WAIT_RXQ_NONEMPTY:
				case TCP_WAIT_ACQ_NONEMPTY:
					events[ret].events |= EPOLLIN;
					break;

				case TCP_WAIT_TXQ_HALF_EMPTY:
				case TCP_WAIT_CON_ESTABLISHED:
					events[ret].events |= EPOLLOUT;
					break;

				case TCP_WAIT_ERROR:
					events[ret].events |= EPOLLERR;
					break;
				}
			// Toogle event bit off
			ev ^= e;
		}
		events[ret].data.fd = s->sockfd;
		ret++;
		it++;
		
		//Clean one-shot events
		evnt->event &= ~(TCP_WAIT_FIN_RECEIVED | TCP_WAIT_CON_ESTABLISHED | TCP_WAIT_ERROR); 
		
		//If no other elements, remove from event queue
		if (evnt->event == 0){
			TCPInfo::epoll_eq_erase(pid, epfd, evnt);
			s->event = NULL;
			delete(evnt);
		}
	}

# if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
# endif
	return ret;
}

int 
TCPSocket::epoll_close(int pid, int epfd)
{
# if CLICK_STATS >= 2
	click_cycles_t start_cycles = click_get_cycles();
	click_cycles_t delta = 0;
# endif
	errno = 0;

	// Check if pid exists
	if (unlikely(!TCPInfo::pid_valid(pid))) {
		errno = EINVAL;
		return -1;
	}

	// Check if epfd is valid
	if (unlikely(!TCPInfo::epoll_fd_valid(epfd))) {
		errno = EBADF;
		return -1;
	}

 	if (unlikely(!TCPInfo::epoll_fd_exists(pid, epfd))) {
		errno = EBADF;
		return -1;
	}
	
	//Clean TCPEvent queue and TCPStates associated
	TCPEventQueue::iterator it = TCPInfo::epoll_eq_begin(pid, epfd);
	TCPEventQueue::iterator e = TCPInfo::epoll_eq_end(pid, epfd);
	while ( it!=e ){
		TCPEvent* evnt = &(*it);
		TCPInfo::epoll_eq_erase(pid, epfd, evnt);
		evnt->state->event = NULL;
		evnt->state->epfd = -1;
		delete(evnt);
	}
	
	// Close epfd
	TCPInfo::epoll_fd_put(pid, epfd);

# if CLICK_STATS >= 2
	delta += (click_get_cycles() - start_cycles);
	_socket->_static_calls += 1;
	_socket->_static_cycles += delta;
# endif
	return 0;
}
#endif /* HAVE_ALLOW_EPOLL */

int
TCPSocket::h_socket(int, String& s, Element *e, 
                               const Handler *, ErrorHandler *errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int domain = -1;
	int type = -1;
	int protocol = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("DOMAIN", domain)
	    .read_mp("TYPE", type)
	    .read_mp("PROTOCOL", protocol)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (domain < 0)
		return errh->error("invalid DOMAIN");
	if (type < 0)
		return errh->error("invalid TYPE");
	if (protocol < 0)
		return errh->error("invalid PROTOCOL");

	int sockfd = t->socket(pid, domain, type, protocol);

	s = "RETVAL " + String(sockfd) + '\n';

	return 0;
}

int
TCPSocket::h_bind(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int sockfd = -1;
	uint16_t port = 0;
	IPAddress addr;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .read_p("ADDRESS", addr)
	    .read_p("PORT", port)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");

	int ret = t->bind(pid, sockfd, addr, port);

	s = "RETVAL " + String(ret) + ", " + \
	    "ADDRESS " + addr.unparse() + ", " + \
	    "PORT " + String(port) + ", " + '\n';

	return 0;
}

int
TCPSocket::h_listen(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int sockfd = -1;
	int backlog = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .read_mp("BACKLOG", backlog)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");
	if (backlog <= 0)
		return errh->error("invalid BACKLOG");

	int ret = t->listen(pid, sockfd, backlog);

	s = "RETVAL " + String(ret) + '\n';

	return 0;
}

int
TCPSocket::h_accept(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int sockfd = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");

	uint16_t port;
	IPAddress addr;
	int fd = t->accept(pid, sockfd, addr, port);

	s = "RETVAL " + String(fd) + ", " + \
	    "ADDRESS " + addr.unparse() + ", " + \
	    "PORT " + String(port) + '\n';

	return 0;
}

int
TCPSocket::h_connect(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int sockfd = -1;
	uint16_t port = 0;
	IPAddress address;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .read_mp("ADDRESS", address)
	    .read_mp("PORT", port)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");
	if (address.empty())
		return errh->error("invalid ADDRESS");
	if (port == 0)
		return errh->error("invalid PORT");

	int ret = t->connect(pid, sockfd, address, port);

	s = "RETVAL " + String(ret) + '\n';

	return ret;
}

int
TCPSocket::h_close(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = reinterpret_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int sockfd = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");

	int ret = t->close(pid, sockfd);

	s = "RETVAL " + String(ret) + '\n';

	return 0;
}

int
TCPSocket::h_fsync(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	Vector<String> conf;
	cp_argvec(s, conf);

	int pid = -1;
	int sockfd = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");

	int ret = t->fsync(pid, sockfd);

	s = "RETVAL " + String(ret) + '\n';

	return 0;
}

int
TCPSocket::h_send(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	const char *nl = find(s.begin(), s.end(), '\n');
	if (!nl)
		return errh->error("malformed send request");

	Vector<String> conf;
	cp_argvec(s.substring(s.begin(), nl), conf);

	int pid = -1;
	int sockfd = -1;
	int len = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .read_mp("DATALEN", len)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");
	if (len < 0)
		return errh->error("invalid DATALEN");

	const char *msg = (len == (s.end() - nl - 1) ? nl + 1 : NULL);
	int ret = t->send(pid, sockfd, msg, len);

	s = "RETVAL " + String(ret) + '\n';

	return 0;
}

int
TCPSocket::h_recv(int, String& s, Element *e, 
                               const Handler *, ErrorHandler* errh)
{
	TCPSocket *t = static_cast<TCPSocket *>(e);

	const char *nl = find(s.begin(), s.end(), '\n');
	if (!nl)
		return errh->error("malformed recv request");

	Vector<String> conf;
	cp_argvec(s.substring(s.begin(), nl), conf);

	int pid = -1;
	int sockfd = -1;
	int len = -1;

	if (Args(conf, errh)
	    .read_mp("PID", pid)
	    .read_mp("SOCKFD", sockfd)
	    .read_mp("DATALEN", len)
	    .complete() < 0)
		return -1;

	if (pid < 0)
		return errh->error("invalid PID");
	if (sockfd < 0)
		return errh->error("invalid SOCKFD");
	if (len < 0)
		return errh->error("invalid DATALEN");

	char *msg = new char [len];
	int ret = t->recv(pid, sockfd, msg, len);

	s = "RETVAL " + String(ret) + '\n';

	if (ret > 0)
		s += String(msg, len);

	delete [] msg;

	return 0;
}

void
TCPSocket::add_handlers()
{
	int f = Handler::f_read | Handler::f_read_param;

	set_handler("socket", f, h_socket);
	set_handler("bind", f, h_bind);
	set_handler("listen", f, h_listen);
	set_handler("accept", f, h_accept);
	set_handler("connect", f, h_connect);
	set_handler("send", f, h_send);
	set_handler("recv", f, h_recv);
	set_handler("close", f, h_close);
	set_handler("fsync", f, h_fsync);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPSocket)
