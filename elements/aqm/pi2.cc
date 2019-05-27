/*
 * pi2.{cc,hh} -- element implements pi square aqm dequeueing mechanism
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
#include "pi2.hh"
#include <click/standard/storage.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/tcpanno.hh>
#include <algorithm>
#include "pi2info.hh"

#define PI2_DEBUG 0
#if PI2_DEBUG
# define click_assert(x) assert(x)
#else
# define click_assert(x) ((void)(0))
#endif
CLICK_DECLS


PI2::PI2()
    : _timer(this)
{
}

PI2::~PI2()
{
}

int
PI2::check_params(unsigned target_q,
					unsigned stability, ErrorHandler *errh) const
{
    unsigned max_allow_thresh = 0xFFFF;
	if (target_q > max_allow_thresh)
		return errh->error("`target_q' too large (max %d)", max_allow_thresh);
    if (stability > 16 || stability < 1)
		return errh->error("STABILITY parameter must be between 1 and 16");
	return 0;
}

int
PI2::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned target_q;
    unsigned stability = 4;

    String queues_string = String();
    if (Args(conf, this, errh)
	.read_mp("W", _w)
	.read_mp("A", _a)
	.read_mp("B", _b)
    .read_mp("K", _k)
    .read_mp("TSHIFT", _t_shift)
	.read_mp("TARGET", target_q) 
	.read_p("QUEUES", AnyArg(), queues_string)
	.read("QREF", target_q)
	.read("STABILITY", stability)
	.complete() < 0)
	return -1;

    if (check_params(target_q, stability, errh) < 0)
		return -1;

    // check queues_string
    if (queues_string) {
		Vector<String> eids;
		cp_spacevec(queues_string, eids);
		_queue_elements.clear();
	for (int i = 0; i < eids.size(); i++)
		if (Element *e = router()->find(eids[i], this, errh))
			_queue_elements.push_back(e);
		if (eids.size() != _queue_elements.size())
		return -1;
    }

    // OK: set variables
	_target_q = target_q;
    _size.set_stability_shift(stability);
    return 0;
}

int
PI2::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    double a, b, w;
    unsigned target_q;
    unsigned stability = 4;

    String queues_string = String();
    if (Args(conf, this, errh)
	.read_mp("W", w)
	.read_mp("A", a)
	.read_mp("B", b)
	.read_mp("TARGET", target_q)
	.read_p("QUEUES", AnyArg(), queues_string)
	.read("QREF", target_q)
	.read("STABILITY", stability)
	.complete() < 0)
	return -1;

    if (check_params(target_q, stability, errh) < 0)
		return -1;

    if (queues_string)
		errh->warning("QUEUES argument ignored");

    // OK: set variables
	_a = a;
	_b = b;
	_w = w;
	_target_q = target_q;
    _size.set_stability_shift(stability);
    return 0;
}

int
PI2::initialize(ErrorHandler *errh)
{
    // Find the next queues
    _queues.clear();
    _queue_l4s = 0;
    _queue_classic = 0;

    if (!_queue_elements.size()) {
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

    if (_queue_elements.size() == 0)
        return errh->error("no Queues downstream");
    for (int i = 0; i < _queue_elements.size(); i++)
        if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
            _queues.push_back(s);
        else
            errh->error("`%s' is not a Storage element", _queue_elements[i]->name().c_str());
    if (_queues.size() != _queue_elements.size())
        return -1;
    else if (_queues.size() == 2){
        _queue_l4s = _queues[0];
        _queue_classic = _queues[1];
    }

    _size.clear();
	_old_q = 0;
	_p = 0;
    _drops = 0;
    _last_jiffies = 0;

    _timer.initialize(this);
    _timer.schedule_after_msec(_w*1000);
    _T = Timestamp::make_msec(0, 1); //TODO max(1ms, serialization time of 2 MTU);
    return 0;
}

void PI2::cleanup(CleanupStage)
{
    _timer.clear();
}

void
PI2::take_state(Element *e, ErrorHandler *)
{
    PI2 *r = (PI2 *)e->cast("PI2");
    if (!r) return;
        _size = r->_size;
}

int
PI2::queue_size() const
{
    if (_queue_l4s && _queue_classic)
        return _queue_l4s->size() + _queue_classic->size();
    else {
        int s = 0;
        for (int i = 0; i < _queues.size(); i++)
            s += _queues[i]->size();
        return s;
    }
}

void
PI2::run_timer(Timer *)
{
    
	_p = _a*_w*(_cur_q - _target_q) + _b*_w*(_cur_q - _prev_q) + _p;
    
    _timer.reschedule_after_msec(_w*1000);
}

Packet *PI2::mark(Packet *p){
    const click_ip *ip = p->ip_header();
    const click_tcp *th = p->tcp_header();
    click_assert(ip && th);
    if((ip->ip_tos & IP_ECNMASK) != IP_ECN_NOT_ECT){
        WritablePacket *q =  p->uniqueify();
        click_ip *ipq = q->ip_header();
        ipq->ip_tos |= IP_ECN_ECT2;
        return q;
    }
    return p;
}

bool PI2::ecn(Packet *p){
    const click_ip *ip = p->ip_header();
    const click_tcp *th = p->tcp_header();
    click_assert(ip && th);
    if((ip->ip_tos & IP_ECNMASK) == IP_ECN_ECT2)
        return true;
    return false;
}

inline void
PI2::handle_drop(Packet *p)
{
    if (noutputs() == 1)
        p->kill();
    else
        p->kill();
    _drops++;
}

Packet *
PI2::pull(int)
{
    while (queue_size() > 0) {
        if(Timestamp::now_steady().msecval() - PI2Info::get_lqtime() + _t_shift >= Timestamp::now_steady().msecval() - PI2Info::get_cqtime()){
            Packet *p = input(0).pull();
            Timestamp now = Timestamp::now();
            Timestamp sojourn_time = now - FIRST_TIMESTAMP_ANNO(p);
            if(sojourn_time > _T || _p > click_random())
                return mark(p);
            return p;
        } else {
            Packet *p = input(1).pull();
            if(_p/_k > std::max(click_random(), click_random())){
                if (ecn(p) == 0)
                    handle_drop(p);
                else{
                    return mark(p);
                }
            } else 
                return p;    
        }
    }
    return NULL;
}


// HANDLERS

static String
pi2_read_drops(Element *f, void *)
{
    PI2 *r = (PI2 *)f;
    return String(r->drops());
}

String
PI2::read_parameter(Element *f, void *vparam)
{
    PI2 *pi2 = (PI2 *)f;
    StringAccum sa;
    switch ((long)vparam) {
      case 3:			// _target_q
	return String(pi2->_target_q);
      case 4:			// stats
	sa << pi2->queue_size() << " current queue\n"
	   << pi2->_size.unparse() << " avg queue\n"
	   << pi2->drops() << " drops\n"
#if CLICK_STATS >= 1
	   << pi2->output(0).npackets() << " packets\n"
#endif
	    ;
	return sa.take_string();
      case 5:			// queues
	for (int i = 0; i < pi2->_queue_elements.size(); i++)
	    sa << pi2->_queue_elements[i]->name() + "\n";
	return sa.take_string();
      default:
	sa << pi2->_a << ", " << pi2->_b << ", " << pi2->_w << ", " << pi2->_target_q
	   << ", QUEUES";
	for (int i = 0; i < pi2->_queue_elements.size(); i++)
	    sa << ' ' << pi2->_queue_elements[i]->name();
	sa << ", STABILITY " << pi2->_size.stability_shift();
	return sa.take_string();
    }
}

void
PI2::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_read_handler("w", read_keyword_handler, "1 W");
    add_write_handler("w", reconfigure_keyword_handler, "1 W");
    add_read_handler("a", read_keyword_handler, "2 A");
    add_write_handler("a", reconfigure_keyword_handler, "2 A");
    add_read_handler("b", read_keyword_handler, "3 B");
    add_write_handler("b", reconfigure_keyword_handler, "3 B");
    add_read_handler("avg_queue_size", read_parameter, 3);
    add_read_handler("stats", read_parameter, 4);
    add_read_handler("queues", read_parameter, 5);
    add_read_handler("config", read_parameter, 6);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(int64)
EXPORT_ELEMENT(PI2)
