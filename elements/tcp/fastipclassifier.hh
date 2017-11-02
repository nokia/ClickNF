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

#ifndef CLICK_FASTIPCLASSIFIER_HH
#define CLICK_FASTIPCLASSIFIER_HH
#include <click/element.hh>
#include <clicknet/ip.h>
#include <click/args.hh>
#include <click/straccum.hh>
#include "fastclassifier.hh"
CLICK_DECLS


class FastIPClassifier : public Element {
  
    void devirtualize_all() { }
  
 public:
  
    FastIPClassifier() { }
    ~FastIPClassifier() { }
    
    const char *class_name() const { return "FastIPClassifier"; }
    const char *port_count() const { return "1/-"; }
    const char *processing() const { return PUSH; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    
    inline Vector<rule> parse_rule(String);
    void push(int, Packet *) final;

    Vector<Vector<rule> > rules;
    
    inline int match(const Packet *p);
};

inline int 
FastIPClassifier::match(const Packet *p)
{
    int l = p->network_length();
    
    if (l > (int) p->network_header_length())
	l += 512 - p->network_header_length();
    else
	l += 256;
    
    if (l < 276)
	return rules.size(); //Discard Packet sending it to max port + 1
    
    const uint8_t *net_data = reinterpret_cast<const uint8_t *>(p->network_header());
    //NOTE Send to first port matching.
    int i;
    for( i = 0; i<rules.size(); i++){
	bool matching = true;
	for(int j = 0; j<rules[i].size(); j++){
	    const uint32_t* d = reinterpret_cast<const uint32_t *>(net_data+rules[i][j].offset);
//            click_chatter ("%d: Data %" PRIu32 " Mask %" PRIu32 " result %" PRIu32 " ",j,(*d),rules[i][j].mask, rules[i][j].result);
            if ( ((*d) & rules[i][j].mask) != rules[i][j].result){
                matching = false;
                break;
            }
	}
	if (matching)
	    return i;
    }
    return i;
}

inline Vector<rule>
FastIPClassifier::parse_rule(String s)
{
    rule r;
    Vector<rule> port_rules;
    
    r.mask = 0;
    r.result = 0;
    port_rules.resize(6,r);
    
    int pos=0;
    String ss = s;
//    int level = 0;

    int next=0;
    String cmd;
    ss.append(" ");
    while (pos < ss.length()) {
	next = ss.find_left(' ',pos);
	if (next == -1)
	    break;
	cmd = ss.substring(pos, next-pos); 
	pos = next+1; //Advance current position
	
	if ( cmd == "tcp" ){
	    port_rules[2].mask += 65280;    //check for TYPE in IP HEADER
	    port_rules[2].result += ((uint32_t) IP_PROTO_TCP << 8); //
	    continue;
	}
	
	if ( cmd == "udp" ){
	    port_rules[2].mask += 65280;    //check for TYPE in IP HEADER
	    port_rules[2].result += ((uint32_t) IP_PROTO_UDP << 8); //
	    continue;
	}
	
	if ( cmd == "src" ){
	    next = ss.find_left(' ',pos);
	    if (next == -1){
		click_chatter("ERROR: FastIPClassifier src need additional parameter, skipping...");
		break;
	    }
	    cmd = ss.substring(pos, next-pos); 
	    pos = next+1; //Advance current position
	    if (cmd == "host"){
		next = ss.find_left(' ',pos);
		if (next == -1){
		    click_chatter("ERROR: FastIPClassifier src host need additional parameter, skipping...");
		    break;
		}
		cmd = ss.substring(pos, next-pos); 
		pos = next+1; //Advance current position
		port_rules[3].result = IPAddress(cmd).addr();
		port_rules[3].mask = (((uint64_t)1<<32)-1);
	    }
	    //port should go here
	}
	
	if ( cmd == "dst" ){
	    next = ss.find_left(' ',pos);
	    if (next == -1){
		click_chatter("ERROR: FastIPClassifier dst need additional parameter, skipping...");
		break;
	    }
	    cmd = ss.substring(pos, next-pos); 
	    pos = next+1; //Advance current position
	    if (cmd == "host"){
		next = ss.find_left(' ',pos);
		if (next == -1){
		    click_chatter("ERROR: FastIPClassifier dst host need additional parameter, skipping...");
		    break;
		}
		cmd = ss.substring(pos, next-pos);
		pos = next+1; //Advance current position
		port_rules[4].result = IPAddress(cmd).addr();
		port_rules[4].mask = (((uint64_t)1<<32)-1);
	    }
	    //port should go here
	}
    }
    
    Vector<rule> final_port_rules;
    for(int j = 0; j<port_rules.size(); j++){
	//Save rules that have a non 0 mask
	if (port_rules[j].mask){
	    port_rules[j].offset = j*4;       
	    final_port_rules.push_back(port_rules[j]);
// 	    click_chatter ("%d: Mask %" PRIu32 " result %" PRIu32 " ",j,port_rules[j].mask, port_rules[j].result);
	}
    }
    return final_port_rules;
}



CLICK_ENDDECLS
#endif

