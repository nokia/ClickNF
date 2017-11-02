/*
 * tcpbulkserver.{cc,hh} -- a bulk transfer server over TCP using zero copy API
 * Rafael Laufer
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


#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpbulkserver.hh"
#include "util.hh"
CLICK_DECLS

TCPBulkServer::TCPBulkServer()
	: _task(this), _length(0), _buflen(0), _batch(0), _verbose(false)
{
}

int
TCPBulkServer::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_batch = 128;
	String buflen = "64K";
	if (Args(conf, this, errh)
		.read_mp("ADDRESS", _addr)
		.read_mp("PORT", _port)
		.read("BUFLEN", buflen)
		.read("BATCH", _batch)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	int b_shift = get_shift(buflen);

	if (!IntArg().parse(buflen, _buflen) || _buflen == 0)
		return errh->error("BUFLEN must be a positive integer");

	_buflen <<= b_shift;

	return 0;
}

int
TCPBulkServer::initialize(ErrorHandler *errh)
{
	int r = TCPApplication::initialize(errh);
	if (r < 0)
		return r;

	ScheduleInfo::initialize_task(this, &_task, errh);

	return 0;
}



void
TCPBulkServer::push(int, Packet *p)
{
	output(0).push(p);
}

bool
TCPBulkServer::run_task(Task *)
{
	int err = 0, fd;
	uint16_t  port;
	IPAddress addr;
	char *msg = new char [_buflen];
	assert(msg);

	fd = click_socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return fd;
	}
	if (_verbose)
		click_chatter("%s: got sockfd %d", class_name(), fd);

	// Bind
	err = click_bind(fd, _addr, _port);
	if (err) {
		perror("bind");
		return err;
	}
	if (_verbose)
		click_chatter("%s: bounded to %s, port %d", \
		                          class_name(), _addr.unparse().c_str(), _port);

	// Listen
	err = click_listen(fd, 1);
	if (err) {
		perror("listen");
		return false;
	}
	if (_verbose)
		click_chatter("%s: listening", class_name());

	// Accept
	int sockfd = click_accept(fd, addr, port);
	if (sockfd == -1) {
		perror("accept");
		return false;
	}
	if (_verbose)
		click_chatter("%s: accepted, sockfd = %d", class_name(), sockfd);

	Timestamp begin = Timestamp::now_steady();
	bool first = true;
	while (true) {
		if (_verbose)
			click_chatter("%s: preparing to pull", class_name());

		Packet *p = click_pull(sockfd, _batch);
		if (!p) {
			perror("pull");
			return false;
		}

		if (p->length() == 0)
			break;

		if (first) {
			first = false;
			begin = Timestamp::now_steady();
		}

		int size = 0;
		int packets = 0;
		while (p) {
			packets++;
			size += p->length();
			_length += p->length();

			Packet *n = p->next();
			checked_output_push(1, p);
			p = n;
		}
		if (_verbose)
			click_chatter("%s: pulled %d packets, %d bytes", class_name(), packets, size);

	}
	Timestamp end = Timestamp::now_steady();

	if (_verbose)
		click_chatter("%s: closing sockfd %d", class_name(), sockfd);
	click_close(sockfd);

	if (_verbose)
		click_chatter("%s: closing sockfd %d", class_name(), fd);
	click_close(fd);

	float rate_mbps = float(_length << 3)/(end - begin).usecval();

	if (rate_mbps < 1000)
		click_chatter("%s: TX rate %.3f Mbps", class_name(), rate_mbps);
	else
		click_chatter("%s: TX rate %.3f Gbps", class_name(), rate_mbps/1000);

	return false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPBulkServer)
ELEMENT_REQUIRES(Util TCPApplication)
