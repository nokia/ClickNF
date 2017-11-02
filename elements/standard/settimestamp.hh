// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SETTIMESTAMP_HH
#define CLICK_SETTIMESTAMP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetTimestamp([TIMESTAMP, I<keyword> FIRST])

=s timestamps

store the time in the packet's timestamp annotation

=d

Store the specified TIMESTAMP in the packet's timestamp annotation. If
TIMESTAMP is not specified, then sets the annotation to the system time when
the packet arrived at the SetTimestamp element.

Keyword arguments are:

=over 8

=item FIRST

Boolean.  If true, then set the packet's "first timestamp" annotation, not its
timestamp annotation.  Default is false.

=back

=a StoreTimestamp, AdjustTimestamp, SetTimestampDelta, PrintOld */

class SetTimestamp : public Element { public:

    SetTimestamp() CLICK_COLD;

    const char *class_name() const		{ return "SetTimestamp"; }
    const char *port_count() const		{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    inline Packet *smaction(Packet *);
    void push(int, Packet *);
    Packet *pull(int);

  private:

    enum { ACT_NOW, ACT_TIME, ACT_FIRST_NOW, ACT_FIRST_TIME }; // order matters
    int _action;
    Timestamp _tv;
    bool _steady;

};

CLICK_ENDDECLS
#endif
