// -*- c-basic-offset: 4 -*-
/*
 * settimestamp.{cc,hh} -- set timestamp annotations
 * Douglas S. J. De Couto, Eddie Kohler
 * based on setperfcount.{cc,hh}
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "settimestamp.hh"
#include <click/args.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
CLICK_DECLS

SetTimestamp::SetTimestamp()
{
}

int
SetTimestamp::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool first = false, delta = false;
    _tv.set_sec(-1);
    _action = ACT_NOW;
    _steady = true;
    if (Args(conf, this, errh)
	.read_p("TIMESTAMP", _tv)
	.read("FIRST", first)
	.read("DELTA", delta)
	.read("STEADY", _steady).complete() < 0)
	return -1;
    if (delta)
	return errh->error("SetTimestamp(DELTA) is deprecated, use SetTimestampDelta(TYPE FIRST)");
    _action = (_tv.sec() < 0 ? ACT_NOW : ACT_TIME) + (first ? ACT_FIRST_NOW : ACT_NOW);
    return 0;
}

Packet *
SetTimestamp::smaction(Packet *p)
{
    if (_action == ACT_NOW){
	if (_steady)
	    p->timestamp_anno().assign_now_steady();
	else
	    p->timestamp_anno().assign_now();
    }
    else if (_action == ACT_TIME)
	p->timestamp_anno() = _tv;
    else if (_action == ACT_FIRST_NOW)
	if (_steady)
	    FIRST_TIMESTAMP_ANNO(p).assign_now_steady();
	else
	    FIRST_TIMESTAMP_ANNO(p).assign_now();
    else
	FIRST_TIMESTAMP_ANNO(p) = _tv;
    return p;
}


void
SetTimestamp::push(int, Packet *p)
{
	if (Packet *q = smaction(p))
		output(0).push(q);
}

Packet *
SetTimestamp::pull(int)
{
	if (Packet *p = input(0).pull())
		return smaction(p);
	else
		return NULL;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTimestamp)
