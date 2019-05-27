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

#include <click/config.h>
#include "ecn_enqueue.hh"
#include <click/standard/storage.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include "pi2info.hh"
CLICK_DECLS

#define ECN_ENQUEUE_DEBUG 0

ECNENQUEUE::ECNENQUEUE()
{
}

ECNENQUEUE::~ECNENQUEUE()
{
}

int
ECNENQUEUE::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned limit;
    String queues_string = String();
    if (Args(conf, this, errh)
	.read_mp("LIMIT", limit)
	.complete() < 0)
        return -1;
    if (limit <= 0)
        return errh->error("Queueing limit must be > 0");
    _limit = limit;
    return 0;
}

int
ECNENQUEUE::initialize(ErrorHandler *errh)
{
    // Find the next queues
    _queues.clear();
    _classical_queue = 0;
    _l4s_queue = 0;

    if (_queue_elements.empty()) {
        ElementCastTracker filter(router(), "Storage");
        int ok;
        if (output_is_push(0))
            ok = router()->visit_downstream(this, 0, &filter);
        else
            ok = router()->visit_upstream(this, 0, &filter);
        if (ok < 0)
            return errh->error("flow-based router context failure");
	_queue_elements = filter.elements();
    }

    if (_queue_elements.empty())
        return errh->error("no nearby Queues");
    for (int i = 0; i < _queue_elements.size(); i++){
        if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
            _queues.push_back(s);
        else
            errh->error("%<%s%> is not a Storage element", _queue_elements[i]->name().c_str());
    }
    if (_queues.size() != _queue_elements.size())
        return -1;
    else if (_queues.size() == 2){
        _classical_queue = _queues[0];
        _l4s_queue = _queues[1];
    }

    return 0;
}

//TODO
void
ECNENQUEUE::take_state(Element *e, ErrorHandler *)
{
    if (ECNENQUEUE *r = (ECNENQUEUE *)e->cast("ECNENQUEUE"))
	_limit = r->_limit;
}

int
ECNENQUEUE::queue_size() const
{
    if (_l4s_queue && _classical_queue)
        return _classical_queue->size() + _l4s_queue->size();
    else {
        int s = 0;
        for (int i = 0; i < _queues.size(); i++)
            s += _queues[i]->size();
        return s;
    }
}

int ECNENQUEUE::cqueue_size() const
{
    if (_classical_queue)
        return _classical_queue->size(); 
     else {
        int s = 0;
        for (int i = 0; i < _queues.size(); i++)
            s += _queues[i]->size();
        return s;
    }
}

int ECNENQUEUE::lqueue_size() const
{
    if (_l4s_queue)
        return _l4s_queue->size(); 
     else {
        int s = 0;
        for (int i = 0; i < _queues.size(); i++)
            s += _queues[i]->size();
        return s;
    }
}

inline void
ECNENQUEUE::handle_drop(Packet *p)
{
    if (noutputs() == 2)
        p->kill();
    else if (noutputs() >2)
        output(2).push(p);
    _drops++;
}

inline bool ECNENQUEUE::ecn_marked(Packet *p)
{
    const click_ip *ip = p->ip_header();
    const click_tcp *th = p->tcp_header();
    if(ip && th){
        if((ip->ip_tos & IP_ECNMASK) == IP_ECN_ECT2)
            return true;
    }
    return false;
}

void
ECNENQUEUE::push(int, Packet *p)
{
    
    if (ecn_marked(p)){
        if(lqueue_size() >= _limit)
            handle_drop(p);
        else {
            PI2Info::set_lqtime((uint32_t)Timestamp::now_steady().msecval());
            output(1).push(p);
        }
    }else{
        if(cqueue_size() >= _limit)
            handle_drop(p);
        else{
            PI2Info::set_cqtime((uint32_t)Timestamp::now_steady().msecval());
            output(0).push(p);
        }
    }
    
}

// HANDLERS

String
ECNENQUEUE::read_handler(Element *f, void *vparam)
{
    ECNENQUEUE *ecn_enqueue = (ECNENQUEUE *)f;
    StringAccum sa;
    switch ((intptr_t)vparam) {
      case 4:			// stats
	sa << ecn_enqueue->queue_size() << "total queue size\n"
       << ecn_enqueue->_limit << "total queue limit\n"
	   << ecn_enqueue->drops() << " drops\n"
#if CLICK_STATS >= 1
	   << ecn_enqueue->output(0).npackets() << "classical packets\n"
       << ecn_enqueue->output(1).npackets() << "l4s packets\n"
#endif
	    ;
	return sa.take_string();
      case 5:			// queues
	for (int i = 0; i < ecn_enqueue->_queue_elements.size(); i++)
	    sa << ecn_enqueue->_queue_elements[i]->name() << "\n";
	return sa.take_string();
      default:			// config
	for (int i = 0; i < ecn_enqueue->_queue_elements.size(); i++)
	    sa << ' ' << ecn_enqueue->_queue_elements[i]->name();
	return sa.take_string();
    }
}

void
ECNENQUEUE::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_read_handler("limit", read_keyword_handler, "0 LIMIT");
    add_write_handler("limit", reconfigure_keyword_handler, "0 LIMIT");
    add_read_handler("stats", read_handler, 4);
    add_read_handler("queues", read_handler, 5);
    add_read_handler("config", read_handler, 6);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(int64)
EXPORT_ELEMENT(ECNENQUEUE)
