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

#ifndef CLICK_FASTCLASSIFIER_HH
#define CLICK_FASTCLASSIFIER_HH
#include <click/element.hh>
#include <clicknet/ip.h>
#include <click/args.hh>
#include <click/straccum.hh>

CLICK_DECLS

struct Rule {
    uint32_t mask;
    uint32_t result;
    uint16_t offset;
};

typedef Rule rule;

class FastClassifier : public Element {

    void devirtualize_all() { }

 public:

    FastClassifier() { }
    ~FastClassifier() { }

    const char *class_name() const { return "FastClassifier"; }
    const char *port_count() const { return "1/-"; }
    const char *processing() const { return PUSH; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    inline Vector<rule> parse_rule(String);
    void push(int, Packet *) final;

    Vector<Vector<rule> > rules; 

    inline int match(const Packet *p);
};

inline int
FastClassifier::match(const Packet *p)
{
    if (p->length() < 22)
        return rules.size();

    const uint8_t *data = reinterpret_cast<const uint8_t *>(p->data());
    //NOTE Send to first port matching.
    int i;
    for( i = 0; i<rules.size(); i++){
        bool matching = true;
        for(int j = 0; j<rules[i].size(); j++){
            const uint32_t* d = reinterpret_cast<const uint32_t *>(data+rules[i][j].offset);
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
FastClassifier::parse_rule(String s)
{
    Vector<rule> port_rules;

    int pos=0;
    String ss = s;
//    int level = 0;

    int next=0;
    String cmd;
    ss.append(" ");
    while (pos < ss.length()) {
        rule r;
        next = ss.find_left(' ',pos);
        if (next == -1)
            break;
        cmd = ss.substring(pos, next-pos);
        pos = next+1; //Advance current position

        int p = 0;
        int n = 0;
        cmd.append("/");
        n = cmd.find_left('/',p);
        if (n == -1)
            break;

        //Set offset
        r.offset = atoi(cmd.substring(p, n-p).c_str());
        p = n+1; //Advance current position

        n = cmd.find_left('/',p);
        if (n == -1)
            break;
        String res = cmd.substring(p, n-p).c_str();
        r.result = strtol(res.c_str(),NULL,16);

        p = n+1; //Advance current position

        switch(res.length()){
            case 1:
                r.mask = (((uint64_t)1<<4)-1);
                break;
            case 2:
                r.mask = (((uint64_t)1<<8)-1);
                r.result = ntohs((uint16_t)r.result);
                break;
            case 3:
                r.mask = (((uint64_t)1<<12)-1);
                r.result = ntohs(r.result);
                break;
            case 4:
                r.mask = (((uint64_t)1<<16)-1);
                r.result = ntohs(r.result);
                break;
            default:
                continue;
        }
        port_rules.push_back(r);
    }
/*    for(unsigned int j = 0; j<port_rules.size(); j++){
        click_chatter ("%d: Mask %" PRIu32 " result %" PRIu32 " ",j,port_rules[j].mask, port_rules[j].result);
    }*/
    return port_rules;
}




CLICK_ENDDECLS
#endif


                                               
