// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_ECN_ENQUEUE_HH
#define CLICK_ECN_ENQUEUE_HH
#include <click/element.hh>
CLICK_DECLS
class Storage;


class ECNENQUEUE : public Element { public:


    ECNENQUEUE() CLICK_COLD;
    ~ECNENQUEUE() CLICK_COLD;

    const char *class_name() const		{ return "ECNENQUEUE"; }
    const char *port_count() const		{ return "1/2"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int queue_size() const;
    int cqueue_size() const;
    int lqueue_size() const;
    int drops() const				{ return _drops; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void take_state(Element *e, ErrorHandler *errh);
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    bool should_drop();
    void handle_drop(Packet *);
    bool ecn_marked(Packet *);
    void push(int port, Packet *);

  protected:

    Storage *_classical_queue;
    Storage *_l4s_queue;
    Vector<Storage *> _queues;

    unsigned _limit;
    unsigned _drops;

    Vector<Element *> _queue_elements;


    static String read_handler(Element *, void *) CLICK_COLD;

    

};

CLICK_ENDDECLS
#endif
