/*
 * batcher.{cc,hh} -- collects N packets and push them to output as a Packets' linked list.
 * Massimo Gallo
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
#include "batcher.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

thread_local Packet* Batcher::head = NULL;
thread_local Packet* Batcher::tail = NULL;
thread_local uint16_t Batcher::len = 0;
TCPTimer* Batcher::txbatch_timers;


Batcher::Batcher()
{
}

int
Batcher::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _size = 32;
    _drain = 1;
    if (Args(conf, this, errh)
	.read("SIZE", _size)
	.read("DRAIN", _drain)
	.complete() < 0)
	return -1;

    uint16_t threads = master()->nthreads();
    txbatch_timers = new TCPTimer[threads];
    for (unsigned int c = 0; c < threads ; c++){
	txbatch_timers[c].assign(txbatch_timer_hook, NULL);
	txbatch_timers[c].initialize(this, c);
    }
    
    return 0;
}

void
Batcher::txbatch_timer_hook(TCPTimer *t, void *data __attribute__((unused)))
{
    Packet * p = head;
    
    len = 0;
    head = NULL;
    tail = NULL;
    
    t->element()->output(0).push(p);
}

void
Batcher::push(int, Packet *p)
{  
#if HAVE_BATCH
    unsigned c = click_current_cpu_id();
    if (!txbatch_timers[c].scheduled())
	txbatch_timers[c].schedule_after_msec(_drain);

    if (head == NULL)
	head = p;
    else
	tail->set_next(p);
    
    p->set_next(NULL);
    tail = p;    
    len++;
    
    if (len >= _size){
	Packet * p = head;
	len = 0;
	head = NULL;
	tail = NULL;
	
	txbatch_timers[c].unschedule();
#endif
	output(0).push(p);
#if HAVE_BATCH
  }
#endif
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Batcher)
ELEMENT_MT_SAFE(Batcher)
