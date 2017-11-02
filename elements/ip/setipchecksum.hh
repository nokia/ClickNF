#ifndef CLICK_SETIPCHECKSUM_HH
#define CLICK_SETIPCHECKSUM_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SetIPChecksum()
 * =s ip
 * sets IP packets' checksums
 * =d
 * Expects an IP packet as input.
 * Calculates the IP header's checksum and sets the checksum header field.
 *
 * You will not normally need SetIPChecksum. Most elements that modify an IP
 * header, like DecIPTTL, SetIPDSCP, and IPRewriter, already update the
 * checksum incrementally.
 *
 * =a CheckIPHeader, DecIPTTL, SetIPDSCP, IPRewriter */

class SetIPChecksum : public Element { public:

    SetIPChecksum() CLICK_COLD;
    ~SetIPChecksum() CLICK_COLD;

    const char *class_name() const		{ return "SetIPChecksum"; }
    const char *port_count() const		{ return PORTS_1_1; }
    void add_handlers() CLICK_COLD;

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    Packet *smaction(Packet *p);
    void push(int, Packet *) final;
    Packet *pull(int);

  private:

    unsigned _drops;
    bool _sharedpkt;  // set checksum on shared packet

};

CLICK_ENDDECLS
#endif
