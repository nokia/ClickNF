/*
 * socks4proxy.{cc,hh} -- 	a simple implementation of modular socks proxy. 
 * 
 * 					      _____Proxy_____
 *				        ---> |               | --->
 * Client	<->	TCPEpollServer       | Server-Client |       TCPEpollClient	<->	Server
 * 					<--- |_______________| <---
 * Massimo Gallo
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
#include <click/master.hh>
#include "socks4proxy.hh"
CLICK_DECLS

#define MAX_FDS 8192

Socks4Proxy::Socks4Proxy() : _verbose(false)
{
}

int
Socks4Proxy::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
		.read("VERBOSE", _verbose)
		.read("PID", _pid)
		.complete() < 0)
		return -1;

	return 0;
}

int
Socks4Proxy::initialize(ErrorHandler *errh)
{
	TCPApplication::initialize(errh);

	//_socketTable.resize(MAX_FDS); 
	// Get the number of threads
	_nthreads = master()->nthreads();
	_socketTable = Vector<Vector<Socket> >(_nthreads, Vector<Socket>(MAX_FDS, Socket()));

	return 0;
}

void
Socks4Proxy::socket_insert(uint16_t core, int fd, int pair, int status)
{
 
	click_assert(fd < MAX_FDS);
	click_assert(_socketTable[core][fd].pair == -1);

	_socketTable[core][fd] = Socket(pair, status);

}

void
Socks4Proxy::socket_remove(uint16_t core, int fd)
{
	click_assert(fd < MAX_FDS);
	click_assert(_socketTable[core][fd].pair != -1);

	_socketTable[core][fd] = Socket();
}

void
Socks4Proxy::push(int port, Packet *p)
{
	int fd = TCP_SOCKFD_ANNO(p);
	unsigned c = click_current_cpu_id();
	
	if (_verbose)
		click_chatter("%s: Incoming packet for fd %d ",class_name(), fd);
	
	switch(port){
		case SOCKS4PROXY_IN_SRV_PORT:	
			//Check for ADD DEL flags 
			if (TCP_SOCK_ADD_FLAG_ANNO(p)){
				socket_insert(c, fd, -1, S_ESTABLISHED);	
				if (_verbose)
					  click_chatter("%s: Inserted S_ESTABLISHED fd %d",class_name(), fd);
				p->kill();
				return;
			}
			
			if (TCP_SOCK_DEL_FLAG_ANNO(p)) {
				int pair = _socketTable[c][fd].pair;
				//Remove both legs of Proxy from socket table
				if (pair != -1){ 
					//Send empty message to close connection towards Server  (using TCPEpollClient)
					p->clear_annotations();
					SET_TCP_SOCK_DEL_FLAG_ANNO(p);
					SET_TCP_SOCKFD_ANNO(p, pair);

					output(SOCKS4PROXY_OUT_CLI_PORT).push(p);
					
					//Remove socket towards Server
					socket_remove(c, pair);
				}
				else
					p->kill();
				
				socket_remove(c, fd);
				if (_verbose)
					  click_chatter("%s: Removed fd %d",class_name(), fd);
				return;
			}
			
			if (_socketTable[c][fd].status == S_ESTABLISHED) {
				// If there is no pair fd yet, read SOCKS request
				if (unlikely(_socketTable[c][fd].pair == -1)) {
					const unsigned char *data = p->data();

					//Check for SOCKS4 protocol and command
					if (data[0] != 4 || data[1] != 1) {
						errno = ENOTSUP;
						perror("unsupported SOCKS version/command");

						
						//Send empty message to close connection towards Client (using TCPEpollServer)
						p->clear_annotations();
						SET_TCP_SOCK_DEL_FLAG_ANNO(p);
						SET_TCP_SOCKFD_ANNO(p, fd);

						output(SOCKS4PROXY_OUT_SRV_PORT).push(p);
					
						//Remove socket towards Client
						socket_remove(c, fd);
						return;
					}

					// Get the destination IP address and TCP port
					IPAddress addr = IPAddress(*(uint32_t *)&data[4]);
					uint16_t port = ntohs(*(uint16_t *)&data[2]);
					
					//Create a Socket and signal it to the client -
					int sockfd = click_socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
					if (sockfd < 0) {
						perror("socket");
						socket_remove(c, fd);
						click_close(fd);
						p->kill();
						return;
					}
					
					// Set LINGER option
					struct linger lin = { .l_onoff = 1, .l_linger = 0 };
					if (click_setsockopt(sockfd, SOL_SOCKET, SO_LINGER,(void*) (&lin),sizeof(lin))){
						perror("setsockopt");
						socket_remove(c, fd);
						click_close(fd);
						p->kill();
						return;
					}
					
					// Update tables
					_socketTable[c][fd].pair = sockfd;
					socket_insert(c, sockfd, fd, S_CONNECTING);

					if (_verbose)
						click_chatter("%s: SOCKS4 parsed correctly, CONNECTING to (%s,%d) through fd %d",class_name(), addr.unparse().c_str(), port, sockfd);

					p->kill();

					//Send an empty message to open connection toward Server (using TCPEpollClient)
					Packet* q = Packet::make((const void *)NULL, 0);
 
					SET_TCP_SOCK_ADD_FLAG_ANNO(q);
					SET_TCP_SOCKFD_ANNO(q, sockfd);
					SET_TCP_DPORT_ANNO(q, port);
					q->set_dst_ip_anno(addr);

					output(SOCKS4PROXY_OUT_CLI_PORT).push(q);
				}
				// Otherwise, this is an already established socket pair
				else {
					if (!p->length()) {
						p->kill();
						return;
					}
					int wfd = _socketTable[c][fd].pair;
					if (_verbose)
						click_chatter("%s: forwarding packet(%dB) from fd %d to fd %d ",class_name(), p->length(), fd, wfd);
					SET_TCP_SOCKFD_ANNO(p, wfd);
					output(SOCKS4PROXY_OUT_CLI_PORT).push(p);
				}
			}
		break;
		case SOCKS4PROXY_IN_CLI_PORT:   
		  
			//Check for ADD DEL flags 
			if (TCP_SOCK_ADD_FLAG_ANNO(p)){
				perror("Client can't add new proxy entries.");
				p->kill();
				return;
			}
			
			if (TCP_SOCK_DEL_FLAG_ANNO(p)) {
				int pair = _socketTable[c][fd].pair;
				//Remove both legs of Proxy from socket table
				if (pair != -1){ 
					//Send empty message to close connection towards Client  (using TCPEpollServer)
					p->clear_annotations();
					SET_TCP_SOCK_DEL_FLAG_ANNO(p);
					SET_TCP_SOCKFD_ANNO(p, pair);

					output(SOCKS4PROXY_OUT_SRV_PORT).push(p);
					
					//Remove socket towards Server
					socket_remove(c,pair);
				}
				else
					p->kill();
				
				socket_remove(c, fd);
				return;
			}
			
			if (TCP_SOCK_OUT_FLAG_ANNO(p)){
				_socketTable[c][fd].status = S_ESTABLISHED;
				if (_verbose)
						click_chatter("%s: SOCKS4, 2 legs connected through fd %d, %d",class_name(), fd, _socketTable[c][fd].pair );
				p->kill();
				
				// Notify client that its request is granted
				WritablePacket *q = Packet::make(TCP_HEADROOM, NULL, 0, 8);
				
				char msg[8] = { 0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
				q = q->push(8);
				memcpy((void *)q->data(), msg, 8);
				SET_TCP_SOCKFD_ANNO(q, _socketTable[c][fd].pair);
				output(SOCKS4PROXY_OUT_SRV_PORT).push(q);
				return;
			}
			
			if (!p->length()) {
				p->kill();
				return;
			}
			int wfd = _socketTable[c][fd].pair;
			if (_verbose)
				click_chatter("%s: forwarding packet(%dB) from fd %d to fd %d ",class_name(), p->length(), fd, wfd);
			SET_TCP_SOCKFD_ANNO(p, wfd);
			output(SOCKS4PROXY_OUT_SRV_PORT).push(p);
		break;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Socks4Proxy)
