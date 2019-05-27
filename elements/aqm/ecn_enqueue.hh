/*
 * ecn_enqueue.{cc,hh} -- enqueue packets based on ECN marking
 * Myriana Rifai
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
