/*
 * dpdk.{cc,hh} -- interface with Intel DPDK (user-level)
 * Rafael Laufer, Diego Perino, Massimo Gallo
 *
 * Copyright (c) 2017 Nokia
 *
 */

#ifndef CLICK_DPDK_HH
#define CLICK_DPDK_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/task.hh>
#include <click/master.hh>
#if HAVE_DPDK
# include <rte_ethdev.h>
#endif // HAVE_DPDK
CLICK_DECLS

#define DPDK_RX_PTHRESH 8
#define DPDK_RX_HTHRESH 8
#define DPDK_RX_WTHRESH 4
#define DPDK_TX_PTHRESH 36
#define DPDK_TX_HTHRESH 0
#define DPDK_TX_WTHRESH 0

#define RSS_HASH_KEY_LENGTH 40

/*
=c

DPDK(ETHER [, I<keywords> MTU, BURST, HW_IP_CHECKSUM, HW_STRIP_CRC])

=s comm

interface with Intel DPDK (user-level)

=d

Reads Ethernet packets from and writes Ethernet packets to an interface
using Intel DPDK. This allows a user-level Click to directly access the 
NIC ring buffers with zero copy.

DPDK expects an Ethernet MAC address (e.g., 00:01:02:03:04:05) as a 
parameter and looks for this particular interface in the list of the
available DPDK interfaces. If such an interface is found, the NIC is 
configured with a per-thread queue.

Keyword arguments are:

=over 8

=item ETHER

Ethernet address. Specifies the DPDK device's Ethernet address. 

=item MTU

Integer. The interface's MTU, including all link headers. Default is 1522 to 
allow 802.1Q tags. Only used if JUMBO_FRAME is enabled.

=item BURST

Integer. The maximum number of packets to emit at a time. Default is 32.

=back

=n

Click must be compiled with DPDK enabled (--enable-dpdk).

=a

FromDevice.u, ToDevice.u, KernelTap  */

class DPDK : public Element { public:

	const char *class_name() const	{ return "DPDK"; }
	const char *port_count() const	{ return "0-1/0-1"; }
	const char *processing() const	{ return AGNOSTIC; }
	const char *flow_code() const	{ return "x/y"; }
	const char *flags() const     { return "S3"; }

#if HAVE_DPDK
	DPDK() CLICK_COLD;
	~DPDK() CLICK_COLD;

	int configure_phase() const { return CONFIGURE_PHASE_PRIVILEGED; }
	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	int initialize(ErrorHandler *) CLICK_COLD;
	void cleanup(CleanupStage) CLICK_COLD;
	void add_handlers() CLICK_COLD;

	bool run_task(Task *);
	void set_active(bool);

	void push(int port, Packet *p);

# if !HAVE_DPDK_PACKET
	inline static bool is_dpdk_packet(Packet* p);
	static void destroy(unsigned char *, size_t, void *);
	static void fake_destroy(unsigned char *, size_t, void *);
# endif // !HAVE_DPDK_PACKET

	inline void set_rate(uint32_t rate);

	static WritablePacket *make(uint32_t headroom, const void *data, 
	                            uint32_t length, uint32_t tailroom);
	inline static WritablePacket *make(const void *data, uint32_t length);
	inline static WritablePacket *make(uint32_t length);

	String print_stats(struct rte_eth_stats stats);

	struct MemPool {
		struct rte_mempool *m;

		MemPool() : m(NULL) { }
	} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
	static MemPool *mempool;

	static uint8_t key[RSS_HASH_KEY_LENGTH];

  private:

	uint16_t tx_batch();
	uint16_t rx_batch();
	WritablePacket *mbuf2packet(struct rte_mbuf *);
	struct rte_mbuf *packet2mbuf(Packet *);
	void print_rss_info();
	void check_link_status();
	static void rx_intr_callback(struct rte_intr_handle *, void *);

//	static uint16_t tx_batch(DPDK *dpdk, unsigned c);
//	static uint16_t rx_batch(DPDK *dpdk, unsigned c);

//	static bool pull_rx_task(Task *, void *);
//	static bool pull_tx_task(Task *, void *);
//	static bool push_rx_task(Task *, void *);
//	static bool push_tx_task(Task *, void *);

	bool _active;
	bool _rx_jumbo_frame;
	bool _rx_strip_crc;
	bool _rx_checksum;
	bool _rx_tcp_lro;
	bool _rx_header_split;
	bool _rx_timestamp_anno;
	bool _rx_mac_hdr_anno;
	bool _rx_pkt_type_anno;
	bool _rx_flow_control;
	bool _rx_scatter;
	bool _tx_flow_control;
	bool _tx_ip_checksum;
	bool _tx_tcp_checksum;
	bool _tx_udp_checksum;
	bool _tx_tcp_tso;

    int _port;
    uint8_t _nthreads;
	uint32_t _rate;
	uint64_t _drain_us;
	uint64_t _drain_tsc;
	uint32_t _burst;
	uint32_t _rx_max_pkt_len;
	uint16_t _rx_split_hdr_size;
	uint32_t _rx_ring_size;
	uint32_t _tx_ring_size;
	uint32_t _speed;
	EtherAddress _macaddr;

	// Statistics
    struct rte_eth_stats _stats;

	class PacketQueue {
	  public:
		PacketQueue() : _head(NULL), _tail(NULL), _size(0) { }

		inline Packet *front() const { return _head; }
		inline Packet *back() const { return _tail; }
		inline bool empty() const { return _size == 0; }
		inline uint32_t size() const { return _size; }
		inline void push_back(Packet *p) {
			p->set_next(NULL);
			if (empty())
				_head = p;
			else
				_tail->set_next(p);
			_tail = p;
			_size++;
		}
		inline void push_front(Packet *p) {
			p->set_next(_head);
			if (empty())
				_tail = p;
			_head = p;
			_size++;
		}
		inline void pop_front() {
			if (_size > 0) {
				Packet *p = front();
				if (p == _tail)
					_tail = NULL;
				_head = p->next();
				_size--;
			}
		}

	  private:
		Packet *_head;
		Packet *_tail;
		size_t _size;
	};

	struct TaskData {
//		Task *tx_task;
//		Task *rx_task;
		Task *task;
		uint64_t prev_tsc;
		uint64_t tx_count;
		uint64_t rx_count;
		PacketQueue tx_pkts;
		PacketQueue rx_pkts;
//		Vector<struct rte_mbuf *> tx_mbuf;
// 		NotifierSignal nonfull_signal;
// 		NotifierSignal nonempty_signal;
// 		ActiveNotifier nonfull_note;
// 		ActiveNotifier nonempty_note;
//		Timer timer;

//		TaskData() : tx_task(NULL), rx_task(NULL), tx_count(0), rx_count(0) { }
		TaskData() : task(NULL), tx_count(0), rx_count(0) { }
	} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
	TaskData *_task;

	static String read_handler(Element *, void *) CLICK_COLD;

#endif // HAVE_DPDK
};

#if HAVE_DPDK
inline void
DPDK::set_rate(uint32_t rate)
{
	_rate = rate;
}

inline WritablePacket *
DPDK::make(const void *data, uint32_t length)
{
	return make(RTE_PKTMBUF_HEADROOM, data, length, 0);
}

inline WritablePacket *
DPDK::make(uint32_t length)
{
	return make(RTE_PKTMBUF_HEADROOM, NULL, length, 0);
}

# if !HAVE_DPDK_PACKET
inline bool
DPDK::is_dpdk_packet(Packet* p)
{
	return p->buffer_destructor() == destroy;
}
# endif // !HAVE_DPDK_PACKET
#endif // HAVE_DPDK

CLICK_ENDDECLS
#endif // CLICK_DPDK_HH
