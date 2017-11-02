/*
 * fastipclassifier.{cc,hh} -- Flexible Fast ip classifier
 * Massimo Gallo, Anandatirtha Nandugudi
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
#include <click/glue.hh>
#include <click/error.hh>
#include "fastclassifier.hh"
CLICK_DECLS


int
FastClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() < noutputs()-1)
	return errh->error("need %d arguments, one per output port (+1 for not matching packets)", noutputs());


    for (int i = 0; i < conf.size(); i++)
	rules.push_back(parse_rule(conf[i]));

    return 0;
}

void
FastClassifier::push(int, Packet *p)
{
    Packet* head = NULL;
    unsigned int r = 0;
#if HAVE_BATCH
    Packet* curr = p;
    Packet* prev = NULL;
    Packet* next = NULL;
    while (curr){
	next = curr->next();
	curr->set_next(NULL);
	r = match(curr);
	//Keep the batch on port 0, unbatch the rest
	if (r == 0){  
	    if (head == NULL)
		head = curr;
	    else
		prev->set_next(curr);
	    prev = curr;
	}
	else{
	    //Not output.port(0) send curr to output port
	    checked_output_push(r, curr);
	}
	curr = next;
    }
    r = 0; //Send the batch to port 0
#else
    head = p;
    r = match(p);
#endif //HAVE_BATCH
    if (head)
	checked_output_push(r, head); 
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FastClassifier)
