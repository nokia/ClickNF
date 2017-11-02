/*
 * tcpbulkclient.{cc,hh} -- a bulk transfer client over TCP
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
#include "tcpbulkclient.hh"
#include "util.hh"
CLICK_DECLS

TCPBulkClient::TCPBulkClient()
	: _task(this), _mss(0), _length(0), _buflen(0), _batch(0), _verbose(false)
{
}

int
TCPBulkClient::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_mss = 1448;
	_batch = 128;
	String length = "0";
	String buflen = "64K";

	if (Args(conf, this, errh)
		.read_mp("ADDRESS", _addr)
		.read_mp("PORT", _port)
		.read("MSS", _mss)
		.read("LENGTH", length)
		.read("BUFLEN", buflen)
		.read("BATCH", _batch)
		.read("VERBOSE", _verbose)
		.complete() < 0)
		return -1;

	if (_mss > 1448)
		return errh->error("MSS out of range");

	int l_shift = get_shift(length);
	int b_shift = get_shift(buflen);

	if (!IntArg().parse(length, _length) || _length == 0)
		return errh->error("LENGTH must be a positive integer");

	if (!IntArg().parse(buflen, _buflen) || _buflen == 0)
		return errh->error("BUFLEN must be a positive integer");

	_length <<= l_shift;
	_buflen <<= b_shift;

	return 0;
}

int
TCPBulkClient::initialize(ErrorHandler *errh)
{
	int r = TCPApplication::initialize(errh);
	if (r < 0)
		return r;

	ScheduleInfo::initialize_task(this, &_task, errh);

	return 0;
}

void
TCPBulkClient::push(int, Packet *p)
{
	output(0).push(p);
}

bool
TCPBulkClient::run_task(Task *task)
{
	int err = 0;
	char *msg = new char [_buflen];
	assert(msg);

	int sockfd = click_socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return false;
	}
	if (_verbose)
		click_chatter("%s: got sockfd %d", class_name(), sockfd);

	// Connect
	err = click_connect(sockfd, _addr, _port);
	if (err == -1) {
		perror("connect");
		return false;
	}
	if (_verbose)
		click_chatter("%s: connected", class_name());

	for (uint32_t i = 0; i < _buflen; i++)
		msg[i] = (char)click_random(33, 125); // ASCII characters

	uint64_t total = 0;
	Timestamp begin = Timestamp::now_steady();
	do {
		uint32_t pkts = 0;
		Packet *p = NULL;
		Packet *t = NULL;
		do {
			Packet *q = Packet::make(TCP_HEADROOM, NULL, _mss, 0);
			if (!q) {
				errno = ENOMEM;
				perror("make");
				return false;
			}
			q->set_next(NULL);

			if (!p)
				p = t = q;
			else {
				t->set_next(q);
				t = q;
			}
			pkts++;
			total += _mss;
			// Send the data to port 1, if something is connected to it
			if (noutputs() > 1)
				output(1).push(q->clone());
		} while ((pkts < _batch) && (total < _length));

		if (_verbose)
			click_chatter("%s: preparing to push", class_name());

		click_push(sockfd, p);
		if (errno) {
			perror("push");
			return false;
		}
		if (_verbose)
			click_chatter("%s: pushed %llu packets, %u bytes", 
			              class_name(), pkts, pkts * _mss); 

		// Let other tasks run
		BlockingTask *u = dynamic_cast<BlockingTask *>(task);
		u->fast_reschedule();
		u->yield(true);
	} while (total < _length);

	click_fsync(sockfd);	
	Timestamp end = Timestamp::now_steady();

	if (_verbose)
		click_chatter("%s: closing sockfd %d", class_name(), sockfd);
	err = click_close(sockfd);
	if (err == -1)
		 perror("close");

	float rate_mbps = float(total << 3)/(end - begin).usecval();

	if (rate_mbps < 1000)
		click_chatter("%s: TX rate %.3f Mbps", class_name(), rate_mbps);
	else
		click_chatter("%s: TX rate %.3f Gbps", class_name(), rate_mbps/1000);

	return false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPBulkClient)
ELEMENT_REQUIRES(Util)
