/*
 * join.{cc,hh} -- joins segments into a packet
 * Rafael Laufer
 *
 * Copyright (c) 2016 Nokia Bell Labs
 *
 */


#ifndef CLICK_JOIN_HH
#define CLICK_JOIN_HH
#include <click/element.hh>
#include <click/packet.hh>
CLICK_DECLS

class Join : public Element { public:

	Join() CLICK_COLD;

	const char *class_name() const		{ return "Join"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	bool can_live_reconfigure() const   { return true; }

	Packet *simple_action(Packet *);

  private:

	uint16_t _segs;
	Packet *_p;

};

CLICK_ENDDECLS
#endif
