/*
 * setipchecksum.{cc,hh} -- element sets IP header checksum
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2012 Eddie Kohler
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
#include "setipchecksum.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
CLICK_DECLS

SetIPChecksum::SetIPChecksum()
    : _drops(0), _sharedpkt(false)
{
}

int
SetIPChecksum::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(this, errh).bind(conf)
        .read("SHAREDPKT", _sharedpkt) 
        .consume() < 0)
        return -1;

    return 0;
}

SetIPChecksum::~SetIPChecksum()
{
}

Packet *
SetIPChecksum::smaction(Packet *p_in)
{
    WritablePacket *p;
    if (_sharedpkt)
	p = (WritablePacket *) p_in;
    else 
	p = p_in->uniqueify();
    
    if (p) {
	unsigned char *nh_data = (p->has_network_header() ? p->network_header() : p->data());
	click_ip *iph = reinterpret_cast<click_ip *>(nh_data);
	unsigned plen = p->end_data() - nh_data, hlen;

	if (likely(plen >= sizeof(click_ip))
	    && likely((hlen = iph->ip_hl << 2) >= sizeof(click_ip))
	    && likely(hlen <= plen)) {
	    iph->ip_sum = 0;
	    iph->ip_sum = click_in_cksum((unsigned char *) iph, hlen);
	    return p;
	}

	if (++_drops == 1)
	    click_chatter("SetIPChecksum: bad input packet");
	p->kill();
    }
    return 0;
}

void
SetIPChecksum::push(int, Packet *p)
{
    Packet* head = NULL;
#if HAVE_BATCH
    Packet* curr = p;
    Packet* prev = p;
    Packet* next = NULL;
    while (curr){
	next = curr->next();
	curr->set_next(NULL);
	
	Packet* r = smaction(curr);
	if (r){    
	    if (head == NULL)
		head = r;
	    else
		prev->set_next(r);
	    prev = r;
	}

	curr = next;
    }
#else
    head = smaction(p);
#endif //HAVE_BATCH
    if (head)
	output(0).push(head);
}

Packet *
SetIPChecksum::pull(int)
{
    //TODO Test
    Packet* p = input(0).pull();
    Packet* head = NULL;
#if HAVE_BATCH
    Packet* curr = p;
    Packet* prev = p;
    Packet* next = NULL;
    while (curr){
	next = curr->next();
	curr->set_next(NULL);
	
	Packet* r = smaction(curr);
	if (r){    
	    if (head == NULL)
		head = r;
	    else
		prev->set_next(r);
	    prev = r;
	}

	curr = next;
    }
#else
    head = smaction(p);
#endif //HAVE_BATCH
    if (head)
      return head;
    else
      return NULL;
}

void
SetIPChecksum::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIPChecksum)
ELEMENT_MT_SAFE(SetIPChecksum)
