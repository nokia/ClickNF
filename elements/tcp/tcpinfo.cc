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

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include "tcpinfo.hh"
#include "tcpstate.hh"
CLICK_DECLS

bool TCPInfo::_verbose(false);
uint64_t TCPInfo::_buckets;
bool TCPInfo::_initialized(false);
uint32_t TCPInfo::_rmem(TCP_RMEM_DEFAULT);
uint32_t TCPInfo::_wmem(TCP_WMEM_DEFAULT);
uint32_t TCPInfo::_usr_capacity(TCP_USR_CAPACITY);
thread_local TCPInfo::SockCount TCPInfo::_usr_sockets(MAX_PIDS,0);
uint32_t TCPInfo::_sys_capacity(TCP_SYS_CAPACITY);
thread_local uint32_t TCPInfo::_sys_sockets;
Vector<IPAddress> TCPInfo::_addr;
uint32_t TCPInfo::_nthreads;
uint32_t TCPInfo::_cong_control(0);

// Per-core port table
TCPInfo::PortTable TCPInfo::_portTable;  // Per-core port table
TCPInfo::SockTable TCPInfo::_sockTable;  // Per-core socket data structures
TCPInfo::SockFDesc TCPInfo::_sockFDesc;  // Per-core socket file descriptors
TCPInfo::FlowTable TCPInfo::_flowTable;  // Per-core flow table
#if HAVE_ALLOW_EPOLL
// Per core epoll data structures
TCPInfo::EpollTable TCPInfo::_epollTable;
TCPInfo::EpollFDesc TCPInfo::_epollFDesc;
#endif

TCPInfo::TCPInfo()
{
}

int
TCPInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (_initialized)
		return errh->error("TCPInfo can only be configured once");

	_verbose = false;

	if (Args(conf, this, errh)
		.read("CONGCTRL", _cong_control)	 
		.read_mp("ADDRS", _addr)
		.read("RMEM", _rmem)
		.read("WMEM", _wmem)
		.read("BUCKETS", _buckets)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	if (_addr.empty())
		return errh->error("ADDRS must be given at least one IP address");
	if (_rmem < TCP_RMEM_MIN)
		return errh->error("RMEM too low");
	if (_rmem > TCP_RMEM_MAX)
		return errh->error("RMEM too high");
	if (_wmem < TCP_WMEM_MIN)
		return errh->error("WMEM too low");
	if (_wmem > TCP_WMEM_MAX)
		return errh->error("WMEM too high");
	
	// Get the number of threads
	_nthreads = master()->nthreads();

	_flowTable = new TCPFlowTable[_nthreads];
	_portTable = new TCPPortTable[_nthreads];
	_sockTable = new TCPSockTable[_nthreads];
	_sockFDesc = new TCPFDesc[_nthreads];
#if HAVE_ALLOW_EPOLL
	_epollFDesc = new TCPFDesc[_nthreads];
	_epollTable = new TCPTable<TCPEventQueue *>[_nthreads];
#endif
	for (unsigned int c = 0; c < _nthreads ; c++){
		if (int r =_flowTable[c].configure(_buckets))
			return r;

		if (int r = _portTable[c].configure(_addr))
			return r;
		_sockTable[c] = TCPSockTable(MAX_PIDS, TCP_USR_CAPACITY, NULL);
		_sockFDesc[c] = TCPFDesc(MAX_PIDS, TCP_USR_CAPACITY, -1, 3);
#if HAVE_ALLOW_EPOLL
		_epollFDesc[c] = TCPFDesc(MAX_PIDS, TCP_USR_CAPACITY, -1, 1);
		_epollTable[c] = TCPTable<TCPEventQueue *>(MAX_PIDS, MAX_EPOLLFD, NULL);
#endif

	}
	
	_usr_sockets.resize(MAX_PIDS, 0);
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPInfo)
