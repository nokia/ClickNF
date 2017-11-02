/*
 * join.{cc,hh} -- joins segments in a packet
 * Rafael Laufer
 *
 * Copyright (c) 2016 Nokia Bell Labs
 *
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include "join.hh"
CLICK_DECLS

Join::Join() : _segs(0), _p(NULL)
{
}

int
Join::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
	    .read_mp("SEGMENTS", _segs)
		.complete() < 0)
		return -1;

	if (_segs == 0 || _segs > 255)
		return errh->error("Invalid number of segments");

	return 0;
}


Packet *
Join::simple_action(Packet *p)
{
	if (_p)
		_p->seg_join(p);
	else
		_p = p;

	if (_p->segments() == _segs) {
		Packet *x = _p;
		_p = NULL;
		return x;
	}

	return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Join)
