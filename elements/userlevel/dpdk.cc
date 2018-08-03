/*
 * dpdk.{cc,hh} -- interface with Intel DPDK (user-level)
 * Rafael Laufer, Diego Perino, Massimo Gallo, Marco Trinelli
 *
 * Copyright (c) 2018 Nokia
 *
 */

#include <click/config.h>
#include <click/glue.hh>
#if HAVE_DPDK
# include <click/args.hh>
# include <click/error.hh>
# include <click/etheraddress.hh>
# include <click/packet_anno.hh>
# include <click/standard/scheduleinfo.hh>
# include <click/packet.hh>
# include <click/straccum.hh>
# include <click/tcpanno.hh>
# include <clicknet/ether.h>
# include <clicknet/ip.h>
# include <clicknet/ip6.h>
# include <clicknet/tcp.h>
# include <clicknet/udp.h>

# include <rte_config.h>
# include <rte_common.h>
# include <rte_eal.h>
# include <rte_ether.h>
# include <rte_lcore.h>
# include <rte_mbuf.h>
# include <rte_cycles.h>
# include <rte_version.h>
# include <rte_errno.h>
# include <rte_ip.h>
# include <rte_pci.h>
#if (RTE_VERSION >= RTE_VERSION_NUM(17,11,0,0))
#include <rte_bus_pci.h>
#endif


# include <string.h>
# include "dpdk.hh"
#endif // HAVE_DPDK
CLICK_DECLS

#if HAVE_DPDK
uint8_t DPDK::key[RSS_HASH_KEY_LENGTH] = {
	0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A
};

bool DPDK::rss_hash_enabled = 0; // by default toeplitz hash computation is not enabled

DPDK::DPDK()
	:   _task(NULL), _nthreads(0), _speed(0)
{
}

DPDK::~DPDK()
{
}

//void *
//DPDK::cast(const char *n)
//{
//	unsigned c = click_current_cpu_id();
//
//	if (strcmp(n, "DPDK") == 0)
//		return (DPDK *)this;
// 	else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
// 		return static_cast<Notifier*>(&_task[c].nonempty_note);
// 	else if (strcmp(n, Notifier::FULL_NOTIFIER) == 0)
// 		return static_cast<Notifier*>(&_task[c].nonfull_note);
//	else
//		return NULL;
//}

int
DPDK::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_active = true;
	_rx_jumbo_frame = false;
	_rx_strip_crc = false;
	_rx_checksum = false;
	_rx_tcp_lro = false;
	_rx_header_split = false;
	_rx_timestamp_anno = true;
	_rx_mac_hdr_anno = true;
	_rx_pkt_type_anno = true;
	_rx_flow_control = true;
	_rx_scatter = true;
	_tx_flow_control = true;
	_tx_ip_checksum = false;
	_tx_tcp_checksum = false;
	_tx_udp_checksum = false;
	_tx_tcp_tso = false;

	_rx_split_hdr_size = 0;
	_rx_max_pkt_len = ETHER_MAX_LEN;

	_burst = 32;
	_tx_ring_size = 512;
	_rx_ring_size = 512;

	_drain_us = 100;

	String speed = "AUTO";

	if (Args(conf, this, errh)
	    .read_mp("ETHER", _macaddr)
	    .read("DRAIN", _drain_us)
	    .read("BURST", _burst)
	    .read("ACTIVE", _active)
	    .read("SPEED", speed)
	    .read("RX_MAX_PKT_LEN", _rx_max_pkt_len)
	    .read("RX_SPLIT_HDR_SIZE", _rx_split_hdr_size)
	    .read("RX_JUMBO_FRAME", _rx_jumbo_frame)
	    .read("RX_STRIP_CRC", _rx_strip_crc)
	    .read("RX_CHECKSUM", _rx_checksum)
	    .read("RX_TIMESTAMP_ANNO", _rx_timestamp_anno)
	    .read("RX_MAC_HDR_ANNO", _rx_mac_hdr_anno)
	    .read("RX_PKT_TYPE_ANNO", _rx_pkt_type_anno)
	    .read("RX_TCP_LRO", _rx_tcp_lro)
	    .read("RX_HEADER_SPLIT", _rx_header_split)
	    .read("RX_FLOW_CONTROL", _rx_flow_control)
	    .read("RX_RING_SIZE", _rx_ring_size)
	    .read("RX_SCATTER", _rx_scatter)
	    .read("TX_RING_SIZE", _tx_ring_size)
	    .read("TX_FLOW_CONTROL", _tx_flow_control)
	    .read("TX_IP_CHECKSUM", _tx_ip_checksum)
	    .read("TX_TCP_CHECKSUM", _tx_tcp_checksum)
	    .read("TX_UDP_CHECKSUM", _tx_udp_checksum)
	    .read("TX_TCP_TSO", _tx_tcp_tso)
	    .read("HASH_OFFLOAD", DPDK::rss_hash_enabled)
		.complete() < 0)
		return -1;

	if (speed == "AUTO")
		_speed = ETH_LINK_SPEED_AUTONEG;
# if RTE_VER_YEAR >= 16
	else if (speed == "10M")
		_speed = ETH_LINK_SPEED_10M;
	else if (speed == "100M")
		_speed = ETH_LINK_SPEED_100M;
	else if (speed == "1G")
		_speed = ETH_LINK_SPEED_1G;
# else
	else if (speed == "10M")
		_speed = ETH_LINK_SPEED_10;
	else if (speed == "100M")
		_speed = ETH_LINK_SPEED_100;
	else if (speed == "1G")
		_speed = ETH_LINK_SPEED_1000;
# endif	
	else if (speed == "10G")
		_speed = ETH_LINK_SPEED_10G;
	else if (speed == "20G")
		_speed = ETH_LINK_SPEED_20G;
	else if (speed == "40G")
		_speed = ETH_LINK_SPEED_40G;
	else
		return errh->error("SPEED must be 10M, 100M, 1G, 10G, 20G, or AUTO");

	if (_rx_max_pkt_len > ETHER_MAX_LEN) {
		if (!_rx_jumbo_frame)
			return errh->error("MTU out of range for non-jumbo frames");
		else if (_rx_max_pkt_len > ETHER_MAX_JUMBO_FRAME_LEN)
			return errh->error("MTU out of range for jumbo frames");
	}
	if (_drain_us > 1000000)
		return errh->error("DRAIN out of range");
	if (_tx_ring_size < 32)
		return errh->error("TX_RING_SIZE out of range");
	if (_rx_ring_size < 32)
		return errh->error("RX_RING_SIZE out of range");
	if (_burst < 32 || _burst > _tx_ring_size || _burst > _rx_ring_size)
		return errh->error("BURST out of range");
# if !HAVE_DPDK_PACKET
	if (_tx_tcp_tso)
		return errh->error("TX_TCP_TSO only valid with DPDK packet");
	if (_rx_tcp_lro)
		return errh->error("RX_TCP_LRO only valid with DPDK packet");
# endif

	// TX queue drained every 100 us by default
	_drain_tsc = _drain_us * ((rte_get_tsc_hz() + US_PER_S - 1)/US_PER_S);

//	int max_socket = 0;
//  #if (RTE_VERSION >= RTE_VERSION_NUM(18,5,0,0))
//	for (int port = 0; port < rte_eth_dev_count_avail(); port++)
//    max_socket = RTE_MAX(max_socket, rte_eth_dev_socket_id(port));
//  #else
//	for (int port = 0; port < rte_eth_dev_count(); port++)
//    max_socket = RTE_MAX(max_socket, rte_eth_dev_socket_id(port));
//  #endif

	// Get the number of threads
	_nthreads = master()->nthreads();

	// Configure tasks
	_task = new TaskData[_nthreads];
	assert(_task);

	// Configure notifiers
// 	for (int c = 0; c < _nthreads; c++) {
// 		TaskData &t = _task[c];
// 
// 		//Initialize queue
// //		t.tx_mbuf.reserve(_burst * 16);
// 
// 		if (input_is_push(0)) {
// 			t.nonfull_note.initialize(Notifier::FULL_NOTIFIER, router());
// 			t.nonfull_note.set_active(true, false);
// 		}
// 
// 		if (output_is_pull(0)) {
// 			t.nonempty_note.initialize(Notifier::EMPTY_NOTIFIER, router());
// 			t.nonempty_note.set_active(true, false);
// 		}
// 	}

	return 0;
}

# if RTE_VERSION >= RTE_VERSION_NUM(17, 8, 0, 0)
static int
#   if RTE_VERSION >= RTE_VERSION_NUM(17, 11, 0, 0)
lsi_event_callback(uint16_t port, enum rte_eth_event_type type, void *param, void *ret_param)
#   else
lsi_event_callback(uint8_t port, enum rte_eth_event_type type, void *param, void *ret_param)  
#   endif
# else
static void
lsi_event_callback(uint8_t port, enum rte_eth_event_type type, void *param)
# endif
{
# if RTE_VERSION >= RTE_VERSION_NUM(17, 8, 0, 0)
	RTE_SET_USED(ret_param);
# endif
	if (type != RTE_ETH_EVENT_INTR_LSC)
# if RTE_VERSION >= RTE_VERSION_NUM(17, 8, 0, 0)
		return 0;
# else
		return;
#endif

	struct rte_eth_link link;
	rte_eth_link_get_nowait(port, &link);

	DPDK *dpdk = static_cast<DPDK *>(param);

	if (link.link_status == 0) {
		click_chatter("%s: port %u down", dpdk->class_name(), uint32_t(port));
		dpdk->set_rate(1);
		dpdk->set_active(false);
# if RTE_VERSION >= RTE_VERSION_NUM(17, 8, 0, 0)
		return 0;
# endif
	}
	else {
		click_chatter("%s: port %u up %d ", dpdk->class_name(), uint32_t(port));
		dpdk->set_rate(1000000/link.link_speed);
		dpdk->set_active(true);
# if RTE_VERSION >= RTE_VERSION_NUM(17, 8, 0, 0)
		return 1;
# endif
	}
}


// TODO Test
// static void 
// rx_intr_callback(void *data)
// {
// // 	// Disable future interrupts
// // 	rte_intr_disable(intr_handle);
// 
// 	// Enable polling (not sure if callback runs in the receiving core)
// 	DPDK *dpdk = static_cast<DPDK *>(data);
// 	unsigned c = click_current_cpu_id();
// 	Task *task = dpdk->_task[c].task;
// 	task->reschedule();
// }

int
DPDK::initialize(ErrorHandler *errh)
{
	int retval, s;
	struct rte_eth_conf port_conf;
	struct rte_eth_dev_info dev_info;
	memset(&port_conf, 0, sizeof(port_conf));

	// Get port index by comparing the MAC address
	_port = -1;
	click_chatter("-------------------------------------------");
    
#if (RTE_VERSION >= RTE_VERSION_NUM(18,5,0,0))
	for (uint8_t port = 0; port < rte_eth_dev_count_avail(); port++) {
#else
    for (uint8_t port = 0; port < rte_eth_dev_count(); port++) {
#endif
		struct ether_addr ether_addr;
		rte_eth_macaddr_get(port, &ether_addr);

		EtherAddress macaddr(ether_addr.addr_bytes);
		click_chatter("%s: port %d, MAC address %s", class_name(), port,
		                                        macaddr.unparse_colon().c_str());
		if (macaddr == _macaddr)
			_port = port;
	}
	click_chatter("-------------------------------------------");

	if (_port == -1)
		return errh->error("unknown MAC address %s", 
		                                      _macaddr.unparse_colon().c_str());

	// Get port info
	rte_eth_dev_info_get(_port, &dev_info);

	// Check TX capabilities
	uint32_t tx_capability = dev_info.tx_offload_capa;
	if (_tx_ip_checksum && !(tx_capability & DEV_TX_OFFLOAD_IPV4_CKSUM))
		return errh->error("no IP checksum offloading TX support "
		                           "for port %d", _port);
	if (_tx_udp_checksum && !(tx_capability & DEV_TX_OFFLOAD_UDP_CKSUM))
		return errh->error("no UDP checksum offloading TX support "
		                            "for port %d", _port);
	if (_tx_tcp_checksum && !(tx_capability & DEV_TX_OFFLOAD_TCP_CKSUM))
		return errh->error("no TCP checksum offloading TX support "
		                            "for port %d", _port);
	if (_tx_tcp_tso) {
		if (!(tx_capability & DEV_TX_OFFLOAD_TCP_TSO))
			return errh->error("no TCP segment offloading support for port %d",
			                                                             _port);
		if (!_tx_ip_checksum || !_tx_tcp_checksum) {
			return errh->error("TCP segment offloading asked for port %d "
			                   "but TCP/IP checksum offloading not set", _port);
		}
	}

	// Check RX capabilities
	uint32_t rx_capability = dev_info.rx_offload_capa;
	if (_rx_checksum && !(rx_capability & DEV_RX_OFFLOAD_IPV4_CKSUM))
		return errh->error("no IP checksum offloading RX support "
		                   "for port %d", _port);
	if (_rx_checksum && !(rx_capability & DEV_RX_OFFLOAD_UDP_CKSUM))
		return errh->error("no UDP checksum offloading RX support "
		                   "for port %d", _port);
	if (_rx_checksum && !(rx_capability & DEV_RX_OFFLOAD_TCP_CKSUM))
		return errh->error("no TCP checksum offloading RX support "
		                   "for port %d", _port);
	if (_rx_tcp_lro) {
		if (!(rx_capability & DEV_RX_OFFLOAD_TCP_LRO))
			return errh->error("no TCP large receive offloading support "
			                       "for port %d", _port);
		if (!_rx_strip_crc) {
			return errh->error("TCP large receive offloading asked for port %d "
			              "but strip CRC not set", _port);
		}
	}

	memset(&port_conf, 0, sizeof(struct rte_eth_conf));
# if RTE_VER_YEAR >= 16
	port_conf.link_speeds = _speed;
# else
	port_conf.link_speed = _speed;
# endif
	port_conf.rxmode.max_rx_pkt_len = _rx_max_pkt_len;
	port_conf.rxmode.split_hdr_size = _rx_split_hdr_size;
	port_conf.rxmode.header_split = _rx_header_split;
	port_conf.rxmode.hw_ip_checksum = _rx_checksum;
	port_conf.rxmode.hw_strip_crc = _rx_strip_crc;
	port_conf.rxmode.jumbo_frame = _rx_jumbo_frame;
	port_conf.rxmode.enable_lro = _rx_tcp_lro;
	port_conf.rxmode.enable_scatter = _rx_scatter;
	port_conf.intr_conf.lsc = 0;
	port_conf.intr_conf.rxq = 0; // Set to 1 to test RX interrupts

	//TODO Verify symmetry
	// Commented,  to force using rss also with 1 thread
	// if (_nthreads > 1) {
    port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
    port_conf.rx_adv_conf.rss_conf.rss_key = key;
    port_conf.rx_adv_conf.rss_conf.rss_key_len = sizeof(key);
    port_conf.rx_adv_conf.rss_conf.rss_hf =
                        ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_SCTP;
	//}

	// Get device socket (only relevant in NUMA architectures)
	s = rte_eth_dev_socket_id(_port);
	if (s > 0 && s != int(rte_socket_id()))
		errh->warning("Port %u is on remote NUMA node", int(_port));

	// Configure interface with one TX/RX queue per thread
	retval = rte_eth_dev_configure(_port, _nthreads, _nthreads, &port_conf);
	if (retval != 0)
		return errh->error("Configure failed");

	struct rte_eth_rxconf rx_conf;
# if RTE_VER_MAJOR >= 2 || RTE_VER_YEAR >= 16
	memcpy(&rx_conf, &dev_info.default_rxconf, sizeof(rx_conf));
# else
	bzero(&rx_conf, sizeof(rx_conf));
# endif
	rx_conf.rx_thresh.pthresh = DPDK_RX_PTHRESH;
	rx_conf.rx_thresh.hthresh = DPDK_RX_HTHRESH;
	rx_conf.rx_thresh.wthresh = DPDK_RX_WTHRESH;
	rx_conf.rx_free_thresh = 32;

	struct rte_eth_txconf tx_conf;
# if RTE_VER_MAJOR >= 2 || RTE_VER_YEAR >= 16
	memcpy(&tx_conf, &dev_info.default_txconf, sizeof(tx_conf));
# else
	bzero(&tx_conf, sizeof(tx_conf));
# endif
	tx_conf.tx_thresh.pthresh = DPDK_TX_PTHRESH;
	tx_conf.tx_thresh.hthresh = DPDK_TX_HTHRESH;
	tx_conf.tx_thresh.wthresh = DPDK_TX_WTHRESH;
	tx_conf.tx_free_thresh = 32;
	tx_conf.tx_rs_thresh = 0;
	if (_tx_tcp_tso)
		tx_conf.txq_flags &= ~ETH_TXQ_FLAGS_NOMULTSEGS;
	else
		tx_conf.txq_flags |= ETH_TXQ_FLAGS_NOMULTSEGS;

	if (_tx_ip_checksum || _tx_tcp_checksum || _tx_udp_checksum)
		tx_conf.txq_flags &= ~ETH_TXQ_FLAGS_NOOFFLOADS;
	else
		tx_conf.txq_flags |= ETH_TXQ_FLAGS_NOOFFLOADS;

	// Set callback for link interrupt
	retval = rte_eth_dev_callback_register(_port, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, (void*)this);
	
	if (retval != 0)
		return errh->error("Callback function registration failed");

	// Delay
	click_chatter("%s: delaying start by 5s", class_name());
	rte_delay_ms(5000);

	// TX/RX queue setup
	for (unsigned i = 0; i < _nthreads; ++i) {
		// RX queue setup
		retval = rte_eth_rx_queue_setup(_port, i, _rx_ring_size, s, 
		                                                &rx_conf, Packet::mempool[i]);
		if (retval < 0)
			return errh->error("RX queue setup failed");

		// TX queue setup
		retval = rte_eth_tx_queue_setup(_port, i, _tx_ring_size, s, &tx_conf);
		if (retval < 0)
			return errh->error("TX queue setup failed");
	}

	// Start notifiers and tasks
	uint64_t tsc =  rte_rdtsc();
	for (unsigned int c = 0; c < _nthreads; c++) {
		TaskData &t = _task[c];

		// Initialize timers
//		t.timer.assign(this);
//		t.timer.initialize(this, c);

		// Initialize timestamp
		t.prev_tsc = tsc;

/*		// Start tasks and notifiers
		if (input_is_push(0))
			t.tx_task = new Task(push_tx_task, this);
		else if (input_is_pull(0)) {
			t.tx_task = new Task(pull_tx_task, this);
			t.nonempty_signal = 
				            Notifier::upstream_empty_signal(this, 0, t.tx_task);
		}
		if (ninputs()) {
			ScheduleInfo::join_scheduler(this, t.tx_task, errh);
			t.tx_task->move_thread(c);
		}

		if (output_is_push(0)) {
			t.rx_task = new Task(push_rx_task, this);
			t.nonfull_signal = \
				           Notifier::downstream_full_signal(this, 0, t.rx_task);
		}
		else if (output_is_pull(0))
			t.rx_task = new Task(pull_rx_task, this);

		if (noutputs()) {
			ScheduleInfo::join_scheduler(this, t.rx_task, errh);
			t.rx_task->move_thread(c);
		}
*/

		t.task = new Task(this);
// 		if (input_is_pull(0))
// 			t.nonempty_signal = Notifier::upstream_empty_signal(this,0,t.task);
// 
// 		if (output_is_push(0))
// 			t.nonfull_signal = Notifier::downstream_full_signal(this,0,t.task);

		ScheduleInfo::initialize_task(this, t.task, true, errh);
		t.task->move_thread(c);

	}

	// Start the interface
	retval = rte_eth_dev_start(_port);
	if (retval < 0)
		return errh->error("Device start failed");

	// Enable promiscuous mode
	rte_eth_promiscuous_enable(_port);

	// Set hardware flow control
	if (_tx_flow_control || _rx_flow_control) {
		struct rte_eth_fc_conf fc_conf;
		retval = rte_eth_dev_flow_ctrl_get(_port, &fc_conf);
		if (retval != 0)
			return errh->error("Flow control get failed");

		fc_conf.low_water = 512 * 60 / 100;   // 60%
		fc_conf.high_water = 512 * 80 / 100;  // 80%
		fc_conf.autoneg = 1;
		fc_conf.send_xon = 1;
		fc_conf.mode = RTE_FC_NONE;

		if (_tx_flow_control && _rx_flow_control)
			fc_conf.mode = RTE_FC_FULL;
		else if (_tx_flow_control)
			fc_conf.mode = RTE_FC_RX_PAUSE;
		else if (_rx_flow_control)
			fc_conf.mode = RTE_FC_TX_PAUSE;

		retval = rte_eth_dev_flow_ctrl_set(_port, &fc_conf);
		if (retval != 0)
			return errh->error("Flow control set failed");
	}

	// Initialize stats
	//
	// NOTE http://dpdk.org/doc/guides/nics/ixgbe.html#statistics
	// The statistics of ixgbe hardware must be polled regularly in order for 
	// it to remain consistent. Running a DPDK application without polling the
	// statistics will cause registers on hardware to count to the maximum value
	// and "stick" at that value. 
	rte_eth_stats_reset(_port);
	rte_eth_stats_get(_port, &_stats);

	click_chatter("%s: port %d, MAC address %s", class_name(), _port,
	                               _macaddr.unparse_colon().c_str());

	// Print RSS information
	// print_rss_info();

	
	// Enable RX interrupt  TODO Test 
// 	struct rte_intr_handle *intr_handle = &dev_info.pci_dev->intr_handle;
// 	retval =  rte_intr_callback_register(intr_handle, rx_intr_callback, (void *)this);
// 	if (retval != 0)
// 		return errh->error("RX callback registration failed");

	check_link_status();

	return 0;
}

void
DPDK::print_rss_info()
{
	// Print redirection table
	struct rte_eth_rss_reta_entry64 reta_conf[8];
	memset(reta_conf, 1, sizeof(reta_conf));
	reta_conf[0].mask = 0xffffffffffffffff;
	reta_conf[1].mask = 0xffffffffffffffff;

	// Get port info
	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(_port, &dev_info);

	if (rte_eth_dev_rss_reta_query(_port, reta_conf, dev_info.reta_size) != 0)
		click_chatter("%s: can't load indirection table", class_name());

	// Print RSS key
	uint16_t idx, shift;
	for (unsigned int r = 0; r < dev_info.reta_size; r++) {
		idx = r / 64;
		shift = r % 64;
		if (!(reta_conf[idx].mask & (1ULL << shift)))
			continue;

		click_chatter("%s: RSS RETA configuration: hash index=%u, queue=%u",
		                           class_name(), r, reta_conf[idx].reta[shift]);
	}

	struct rte_eth_rss_conf rss_conf;
	uint8_t rss_key[10 * 4] = "";
 
	rss_conf.rss_key = rss_key;
	rss_conf.rss_key_len = sizeof(rss_key);

	rte_eth_dev_rss_hash_conf_get(_port, &rss_conf);

	StringAccum sa;
	sa << class_name() << ": RSS KEY: hash key= ";
	for (unsigned int r = 0; r < rss_conf.rss_key_len; r++) {
		String h = String(rss_conf.rss_key[r]).quoted_hex().substring(2, 2);
		sa << "0x" << h << " ";
	}
 	click_chatter("%s", sa.take_string().c_str());
}

void
DPDK::check_link_status()
{
#define CHECK_INTERVAL 100 // 100ms 
#define MAX_CHECK_TIME 100 // 10s 
	struct rte_eth_link link;
	
	set_rate(1);
	set_active(false);
	
	for (int count = 0; count <= MAX_CHECK_TIME; count++) {
		memset(&link, 0, sizeof(link));
		rte_eth_link_get_nowait(_port, &link);

		if (link.link_status) {
			set_rate(1000000/link.link_speed); 
			set_active(true);
			
			click_chatter("%s: port %d, link up, speed %u Mbps, %s %u\n", 
			            class_name(), _port, (unsigned)link.link_speed,
				        (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? 
			                    "full-duplex" : "half-duplex", _rate);
			break;
		}

		rte_delay_ms(CHECK_INTERVAL);
	}

	if (!link.link_status)
		click_chatter("%s: port %d, link down\n", class_name(), _port);
}

void
DPDK::cleanup(CleanupStage)
{
	uint64_t tx_count = 0;
	uint64_t rx_count = 0;

	if (_task) {
		click_chatter("\n");
		for (int c = 0; c < _nthreads; c++) {
			TaskData &t = _task[c];
			click_chatter("tid %d, tx %llu, rx %llu",c, t.tx_count, t.rx_count);

			tx_count += t.tx_count;
			rx_count += t.rx_count;

/*			if (_task[c].tx_task) {
				_task[c].tx_task->unschedule();
				delete _task[c].tx_task;
			}
			if (_task[c].rx_task) {
				_task[c].tx_task->unschedule();
				delete _task[c].rx_task;
			}
*/
			if (_task[c].task) {
				_task[c].task->unschedule();
				delete _task[c].task;
			}
		}
		click_chatter("TOTAL  tx %llu, rx %llu", tx_count, rx_count);
	}

	if (_port != -1) {
		struct rte_eth_stats stats;
		rte_eth_stats_get(_port, &stats);
		rte_eth_stats_reset(_port);

		_stats.ipackets  += stats.ipackets;
		_stats.opackets  += stats.opackets;
		_stats.ibytes    += stats.ibytes;
		_stats.obytes    += stats.obytes;
		_stats.imissed   += stats.imissed;
		_stats.oerrors   += stats.oerrors;
		_stats.rx_nombuf += stats.rx_nombuf;

		StringAccum sa;
		sa << "FINAL: Port " << _port << '\n';
		sa << print_stats(_stats) << '\n';
		click_chatter("%s", sa.take_string().c_str());
	}
}

# if !HAVE_DPDK_PACKET
void
DPDK::destroy(unsigned char *, size_t, void *buf)
{
	rte_pktmbuf_free(static_cast<struct rte_mbuf *>(buf));
}

void
DPDK::fake_destroy(unsigned char *, size_t, void *)
{
}
# endif // !HAVE_DPDK_PACKET

/*
bool
DPDK::pull_rx_task(Task *task, void *data)
{
	DPDK *dpdk = static_cast<DPDK *>(data);
	if (!dpdk->_active)
		return false;

	unsigned c = click_current_cpu_id();
	TaskData &t = dpdk->_task[c];

	uint16_t rx_count = dpdk->rx_batch();

	// If received something, wake up downstream task and do not
	// reschedule this task, since pull() will do it when needed
	if (rx_count)
		t.nonempty_note.wake();
	else
		task->fast_reschedule();

	return rx_count > 0;
}

bool
DPDK::pull_tx_task(Task *task, void *data)
{
	DPDK *dpdk = static_cast<DPDK *>(data);
	if (!dpdk->_active)
		return false;

	unsigned c = click_current_cpu_id();
	TaskData &t = dpdk->_task[c];

	uint16_t tx_count = 0;
	if (t.nonempty_signal || t.tx_mbuf.size()) {
		while (t.tx_mbuf.size() < dpdk->_burst) {
			Packet *p = dpdk->input(0).pull();
			if (!p)
				break;

			// Update TX buffer
			struct rte_mbuf *m = dpdk->packet2mbuf(p);
			if (!m)
				break;

			t.tx_mbuf.push_back(m);
		}

		if (t.tx_mbuf.size()) {
			uint64_t curr_tsc = rte_rdtsc();
			if ((t.tx_mbuf.size() >= dpdk->_burst) ||
			    (curr_tsc - t.prev_tsc > dpdk->_drain_tsc)) {
				tx_count = dpdk->tx_batch();
				t.prev_tsc = curr_tsc;
			}
		}

		task->fast_reschedule();
	}

	return tx_count > 0;
}

bool
DPDK::push_rx_task(Task *task, void *data)
{
	DPDK *dpdk = static_cast<DPDK *>(data);
	if (!dpdk->_active)
		return false;

	unsigned c = click_current_cpu_id();
	TaskData &t = dpdk->_task[c];

	uint16_t rx_count = 0;
	if (t.nonfull_signal) {
		rx_count = dpdk->rx_batch();
		task->fast_reschedule();
	}

	return rx_count > 0;
}

bool
DPDK::push_tx_task(Task *task, void *data)
{
	DPDK *dpdk = static_cast<DPDK *>(data);
	if (!dpdk->_active)
		return false;

	unsigned c = click_current_cpu_id();
	TaskData &t = dpdk->_task[c];

	uint16_t tx_size = t.tx_mbuf.size();

	if (tx_size == 0)
		return false;

	uint16_t tx_count = dpdk->tx_batch();

	if (tx_count == tx_size)
		t.nonfull_note.wake();
	else
		task->fast_reschedule();

	return tx_count > 0;
}
*/

bool
DPDK::run_task(Task *task)
{
	if (unlikely(!_active))
		return false;

	unsigned c = click_current_cpu_id();
	uint16_t tx_count = 0;
	uint16_t rx_count = 0;
	TaskData &t = _task[c];

	// RX
/*	if (output_is_pull(0) && !t.nonempty_note.active()) {
		// If the RX buffer was previously empty, wake up downstream task
		// and do not reschedule this task, as pull() will do so when needed
		rx_count = rx_batch();
		if (rx_count)
			t.nonempty_note.wake();
		else
			task->fast_reschedule();
	}
	else */if (output_is_push(0) /*&& t.nonfull_signal*/) {
		rx_count = rx_batch();
		task->fast_reschedule();
	}

	// TX
//	if (input_is_push(0) && !t.nonfull_note.active()) {
//		uint16_t tx_size = t.tx_pkts.size();
//		tx_count = tx_batch();
//		if (tx_count == tx_size)
//			t.nonfull_note.wake();
//		else
//			task->fast_reschedule();
//	}
//	else if (input_is_pull(0) && (t.nonempty_signal || t.tx_pkts.size())) {
//		while (t.tx_pkts.size() < _burst) {
//			Packet *p = input(0).pull();
//			if (!p)
//				break;
//
//			t.tx_pkts.push_back(p);
// //		}
//
//		if (t.tx_pkts.size()) {
//			uint64_t curr_tsc = rte_rdtsc();
//			if ((t.tx_pkts.size() >= _burst) ||
//			    (curr_tsc - t.prev_tsc > _drain_tsc)) {
//				tx_count = tx_batch();
//				t.prev_tsc = curr_tsc;
//			}
//		}
//
//		task->fast_reschedule();
//	}

	// TX
// 	if (input_is_pull(0) && t.nonempty_signal) {
// 		while (t.tx_pkts.size() < _burst) {
// 			Packet *p = input(0).pull();
// 			if (!p)
// 				break;
// 
// 			t.tx_pkts.push_back(p);
// 		}
// 	}
	if (t.tx_pkts.size()) {
		uint64_t curr_tsc = rte_rdtsc();
		if (t.tx_pkts.size() >= _burst || curr_tsc - t.prev_tsc > _drain_tsc) {
			tx_count = tx_batch();
			t.prev_tsc = curr_tsc;
		}

		task->fast_reschedule();
	}

	return (tx_count > 0 || rx_count > 0);
}


//void
//DPDK::run_timer(Timer *)
//{
//	unsigned c = click_current_cpu_id();
//	TaskData &t = _task[c];
//
//	uint16_t tx_size = t.tx_pkts.size();
//	if (tx_batch() < tx_size) {
//		t.nonfull_note.sleep();
//		t.task->reschedule();
////		t.tx_task->reschedule();
//	}
//}

uint16_t
DPDK::tx_batch()
{
	unsigned c = click_current_cpu_id();
	TaskData &t = _task[c];
	uint32_t tx_count = 0;
	do {
		uint16_t tx_size = RTE_MIN(t.tx_pkts.size(), _burst);
		if (tx_size == 0)
			return tx_count;

		struct rte_mbuf *tx_mbuf[tx_size];
		Packet *p = t.tx_pkts.front();

		for (uint32_t i = 0; i < tx_size; i++) {
			// Convert it to an mbuf
			struct rte_mbuf *m = p->packet2mbuf(_tx_ip_checksum, _tx_tcp_checksum, _tx_udp_checksum, _tx_tcp_tso);

			// Update TX buffer
			tx_mbuf[i] = m;

			// Get next packet
			p = p->next();
		}

		uint16_t attempt = 0;
		uint16_t nb_pkts = tx_size;
		struct rte_mbuf **tx_pkts = tx_mbuf;

//		for (int i = 0; i < tx_size; i++)
//    	    rte_prefetch0(rte_pktmbuf_mtod(tx_mbuf[i], void *));

		do {
			uint16_t n = rte_eth_tx_burst(_port, c, tx_pkts, nb_pkts);
			nb_pkts -= n;
			tx_pkts += n;
			attempt += (n == 0);
		} while (nb_pkts > 0 && attempt < 5);

		// Get TX count
		tx_count = (tx_size - nb_pkts);

		// Increase TX counter
		t.tx_count += tx_count;

		// Remove transmitted packets from the TX queue
		for (uint32_t i = 0; i < tx_count; i++) {
# if HAVE_DPDK_PACKET
			t.tx_pkts.pop_front();
# else
			Packet *p = t.tx_pkts.front();
			t.tx_pkts.pop_front();
			
			p->kill();
# endif
		}

		// Get out if TX queue is full
		if (nb_pkts)
			break;

	} while (t.tx_pkts.size());

	return tx_count;
}


uint16_t
DPDK::rx_batch()
{
	unsigned c = click_current_cpu_id();
	struct rte_mbuf *rx_mbuf[_burst];

	// Get a new batch
	uint16_t rx_count = rte_eth_rx_burst(_port, c, rx_mbuf, _burst);

	if (rx_count == 0)
		return 0;

	Timestamp now;
	TaskData &t = _task[c];

	// Getting the current time is costly, do it once per batch
	if (_rx_timestamp_anno)
		now = Timestamp::now_steady();
// 	// Prevent error accumulation in the timestamps
// 	uint32_t epsilon = 0;

	// Prefetching packet annotations and data
// 	for (uint16_t i = 0; i < rx_count; i++) {
// # if HAVE_DPDK_PACKET
// 		// Prefetch annotations
// 		char *c = (char *)(rx_mbuf[i] + 1);
// 		rte_prefetch0(c);
// 		rte_prefetch0(c + CLICK_CACHE_LINE_SIZE);
// # endif
// 		// Prefetch packet payload
// 		rte_prefetch0(rte_pktmbuf_mtod(rx_mbuf[i], void *));
// 	}

	// Create the packets and push them out
#if HAVE_BATCH
	Packet* head = NULL;
	Packet* prev = NULL;
#endif
	for (uint16_t i = 0; i < rx_count; i++) {
		WritablePacket *p = Packet::mbuf2packet(rx_mbuf[i]);
# if HAVE_DPDK_PACKET && HAVE_BATCH 
		// Prefetch annotations and first data cahce line of the next packet 
		// if we have batched output (might not be helpful if we don't have batched output)
                if (i+1 < rx_count){
                        char *c = (char *)(rx_mbuf[i+1] + 1);
                        rte_prefetch0(c);
                        rte_prefetch0(c + CLICK_CACHE_LINE_SIZE);
                        rte_prefetch0(rte_pktmbuf_mtod(rx_mbuf[i+1], void *));
                }
# endif
		// Set packet timestamp annotation
		if (_rx_timestamp_anno) {
			p->set_timestamp_anno(now);

//NOTE 	Espensive operations. Even with mss packets, correction is of 0.21us per packet.
//	Timestamp correction is not useful. We have ms Timeouts, in a batch of 128pkts 
//	we will have mac correction of 26us.
//TODO	Make it optional.
//	
// 			// Compute channel utilization in bytes
// 			uint32_t bytes = 7            // Preamble
// 			               + 1            // Start-of-frame delimiter
// 			               + p->length()  // Header and payload
// 			               + 4            // CRC
// 			               + 12;          // Interframe space (IFS)
// 	
// 			// Transmission time in picoseconds (rate is in Mbps)
// 			uint64_t picosec = uint64_t(bytes << 3) * (_rate);
// 	
// 			// Fix it using accumulated sub-nanosecond error
// 			if (epsilon >= 1000) {
// 				epsilon -= 1000;
// 				picosec += 1000;
// 			}
// 			epsilon += (picosec % 1000);
// 
// 			// Correct current time for next packet
// 			now += Timestamp::make_nsec(0, uint32_t(picosec/1000));
		}

		// Discard packets with bad checksum
		if (_rx_checksum) {
			struct rte_mbuf *m = rx_mbuf[i];
			uint32_t flags = m->ol_flags;
			if (unlikely(flags & (PKT_RX_IP_CKSUM_BAD|PKT_RX_L4_CKSUM_BAD))) {
				p->kill();
				continue;
			}
		}

		// Set packet type annotation
		if (_rx_pkt_type_anno) {
			if (likely((*p->data() & 1) == 0))
				p->set_packet_type_anno(Packet::HOST);
			else {
				if (EtherAddress::is_broadcast(p->data()))
					p->set_packet_type_anno(Packet::BROADCAST);
				else
					p->set_packet_type_anno(Packet::MULTICAST);
			}
		}

		// Set MAC header pointer
		if (_rx_mac_hdr_anno)
			p->set_mac_header(p->data());

		if (output_is_push(0)){
#if HAVE_BATCH
			if (likely(head != NULL))
				prev->set_next(p);
			else
				head = p;
			prev = p;
#else
			output(0).push(p);
#endif
		}
		else
			t.rx_pkts.push_back(p);
	}
	
#if HAVE_BATCH
	output(0).push(head);
#endif

	// Increase RX counter
	t.rx_count += rx_count;

	return rx_count;
}

void
DPDK::push(int, Packet *p)
{
	if (unlikely(!_active)) {
		p->kill();
		return;
	}

	unsigned c = click_current_cpu_id();
	TaskData &t = _task[c];

	// Check for overflow
//	if (unlikely(t.tx_mbuf.size() == t.tx_mbuf.capacity())) {
//		p->kill();
//		click_chatter("DPDK: buffer overflow");
//		return;
//	}

	// Convert packet to mbuf
//	struct rte_mbuf *m = packet2mbuf(p);
//	if (unlikely(m == NULL)) {
//		click_chatter("DPDK: packet2mbuf() failed");
//		return;
//	}

	// Insert packet in the TX queue
//	t.tx_mbuf.push_back(m);
	Packet* head = p;
#if HAVE_BATCH
	Packet* next = NULL;
	while (head){
		next = head->next();
		head->set_next(NULL);
#endif
		t.tx_pkts.push_back(head);
#if HAVE_BATCH
		head = next;
	}
#endif

	// 
//	if (unlikely(uint32_t(t.tx_pkts.size()) >= _burst)) {
//		t.timer.unschedule();
//
//		uint16_t tx_size = t.tx_pkts.size();
//		uint16_t tx_count = tx_batch();
//		if (tx_count < tx_size) {
//			t.nonfull_note.sleep();
//			t.task->reschedule();
////			t.tx_task->reschedule();
//		}
//	}
//	else if (!t.timer.scheduled())
//		t.timer.schedule_after(Timestamp::make_usec(_drain_us));

	uint64_t curr_tsc = rte_rdtsc();
        if (t.tx_pkts.size() >= _burst || curr_tsc - t.prev_tsc > _drain_tsc) {
		tx_batch();
		t.prev_tsc = curr_tsc;
	}



	if (!t.task->scheduled())
		t.task->reschedule();
}

// Packet *
// DPDK::pull(int)
// {
// 	if (unlikely(!_active))
// 		return NULL;
// 
// 	unsigned c = click_current_cpu_id();
// 	TaskData &t = _task[c];
// 
// 	if (t.rx_pkts.empty() && rx_batch() == 0) {
// 		t.nonempty_note.sleep();
// 		t.task->reschedule();
// //		t.tx_task->reschedule();
// 		return NULL;
// 	}
// 
// 	Packet *p = t.rx_pkts.front();
// 	t.rx_pkts.pop_front();
// 
// 	return p;
// }

void
DPDK::set_active(bool active)
{
	_active = active;

	for (int c = 0; c < _nthreads; c++) {
		TaskData &t = _task[c];

		if (active) {
//			if (t.rx_task && !t.rx_task->scheduled())
//				t.rx_task->reschedule();
//
//			if (t.tx_task && !t.tx_task->scheduled())
//				t.tx_task->reschedule();

			if (t.task && !t.task->scheduled())
				t.task->reschedule();

// 			if (output_is_pull(0))
// 				t.nonempty_note.wake();
// 			else if (output_is_push(0))
// 				t.nonfull_note.wake();

		}
		if (!active) {
//			if (t.rx_task && t.rx_task->scheduled())
//				t.rx_task->unschedule();
//
//			if (t.tx_task && t.tx_task->scheduled())
//				t.tx_task->unschedule();

			if (t.task && t.task->scheduled())
				t.task->unschedule();

// 			if (output_is_pull(0))
// 				t.nonempty_note.sleep();
// 			else if (output_is_push(0))
// 				t.nonfull_note.sleep();
		}
	}
}

String
DPDK::print_stats(struct rte_eth_stats stats)
{
	StringAccum sa;
	sa << " RX-packets: " << stats.ipackets;
	sa << " TX-packets: " << stats.opackets;
	sa << " Missed: "     << stats.imissed;
	sa << " Error: "      << stats.oerrors;
	sa << " No mbuf: "    << stats.rx_nombuf;

	return sa.take_string();
}

String
DPDK::read_handler(Element* e, void *id)
{
	StringAccum sa;
	DPDK* dpdk = static_cast<DPDK*>(e);

	struct rte_eth_stats stats;

	rte_eth_stats_get(dpdk->_port, &stats);
	rte_eth_stats_reset(dpdk->_port);
	
	dpdk->_stats.ipackets  += stats.ipackets;
	dpdk->_stats.opackets  += stats.opackets;
	dpdk->_stats.ibytes    += stats.ibytes;
	dpdk->_stats.obytes    += stats.obytes;
	dpdk->_stats.imissed   += stats.imissed;
	dpdk->_stats.oerrors   += stats.oerrors;
	dpdk->_stats.rx_nombuf += stats.rx_nombuf;

	if (id == (void *)0) {
		sa << "Port " << dpdk->_port << '\n';
		sa << dpdk->print_stats(stats) << '\n';
		return sa.take_string();
	}

	return String("");
}

void
DPDK::add_handlers()
{
	add_read_handler("stats", read_handler, 0);
	//TODO add more if needed
	add_read_handler("count", read_handler, 3);
//	add_write_handler("reset_counts", write_handler, 0, Handler::BUTTON);
}


CLICK_ENDDECLS
#endif // HAVE_DPDK_H
ELEMENT_REQUIRES(userlevel dpdk)
EXPORT_ELEMENT(DPDK)

