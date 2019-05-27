// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_PI2_HH
#define CLICK_PI2_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/timer.hh>
CLICK_DECLS
class Storage;

class PI2 : public Element { public:

    // Queue sizes are shifted by this much.
    enum { QUEUE_SCALE = 10 };
    typedef DirectEWMAX<StabilityEWMAXParameters<QUEUE_SCALE, uint64_t, int64_t> > ewma_type;

    PI2() CLICK_COLD;
    ~PI2() CLICK_COLD;

    const char *class_name() const		{ return "PI2"; }
    const char *port_count() const		{ return "2/1"; }
    const char *processing() const		{ return PULL; }

    int queue_size() const;
    const ewma_type &average_queue_size() const { return _size; }
    int drops() const				{ return _drops; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int check_params(unsigned, unsigned, ErrorHandler *) const ;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void take_state(Element *, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    int live_reconfigure(Vector<String> &, ErrorHandler *);
    void add_handlers() CLICK_COLD;

    Packet *mark(Packet *);
    bool ecn(Packet *);
    void handle_drop(Packet *);
    Packet *pull(int);
    void run_timer(Timer *);

  protected:

	Timer _timer;
    Storage *_queue_l4s;
    Storage *_queue_classic;
    Vector<Storage *> _queues;

    ewma_type _size;

    int _random_value;
    int _last_jiffies;

    int _drops;

	double   _p; // _a = 20, _b = 200 in Hz 
	unsigned int _k, _w, _a, _b, _target_q, _old_q, _prev_q, _cur_q, _t_shift; // _k = 2, _w = 32, _target_q = 20 , _t_shift = 40 in ms 
    Timestamp _T;
    
    Vector<Element *> _queue_elements;

    static String read_parameter(Element *, void *);

    static const int MAX_RAND=2147483647;

};

CLICK_ENDDECLS
#endif
