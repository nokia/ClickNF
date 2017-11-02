/*
 * etheripmapencap.{cc,hh} -- encapsulates IP packet in Ethernet header according to the IPDST/ETHDST given in input
 *
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
#include "etheripmapencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

int
EtherIPMapEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _shared = true;
    if (Args(this, errh).bind(conf)
	.read_mp("SRC", EtherAddressArg(), _eths)
	.read("SHARED", _shared)
	.consume() < 0)
	return -1;
    int n = master()->nthreads();
    _ethIpMap = new EthIPMap[n];
    for (int i = 0; i < conf.size(); i++)
	parse_map(conf[i]);
    // Copy Hash table of core 0 to other cores
    for (int i = 1; i < n; i++)
	_ethIpMap[i] = _ethIpMap[0];
    return 0;
}

void
EtherIPMapEncap::parse_map(String s)
{
    int pos=0;
    String ss = s;

    int next=0;
    String cmd;
    ss.append(" ");
    
    next = ss.find_left(' ',pos);
    assert(next != -1);

    cmd = ss.substring(pos, next-pos);
        
    int p = 0;
    int n = 0;
    cmd.append("/");
    
    //Get IPDST
    n = cmd.find_left('/',p);
    assert(n != -1);

    IPAddress ipd = IPAddress(cmd.substring(p, n-p).c_str());
    p = n+1; //Advance current position
    
    n = cmd.find_left('/',p);
    
    uint8_t ethd[6]; 
    EtherAddressArg().parse(cmd.substring(p, n-p).c_str() , ethd);
    EthIPMap::iterator it = _ethIpMap[0].find_insert(ipd);
    memcpy(it.value().data(), ethd, 6);
}

Packet *
EtherIPMapEncap::smaction(Packet *p)
{
    IPAddress ipd= p->dst_ip_anno();
    assert(ipd != IPAddress());
    uint16_t ether_type = htons(0x0800);

    EthIPMap::iterator it = _ethIpMap[click_current_cpu_id()].find(ipd);
    assert( it != _ethIpMap[click_current_cpu_id()].end());
    
    WritablePacket *q;
    if(_shared)
	q = (WritablePacket *) p->nonunique_push(sizeof(click_ether));
    else
	q= p->push_mac_header(14);
      
    if(q) {
	memcpy(q->data(),  it.value().data(), 6);
	memcpy(q->data()+6,  _eths, 6);
	memcpy(q->data()+12,  & ether_type, 2);
	return q;
    } else
	return 0;
}

void
EtherIPMapEncap::push(int, Packet *p)
{
//     if (Packet *q = smaction(p))
// 	output(0).push(q);
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
EtherIPMapEncap::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherIPMapEncap)
