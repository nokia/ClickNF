// -*- related-file-name: "../include/click/packet.hh" -*-
/*
 * packet.{cc,hh} -- a packet structure. In the Linux kernel, a synonym for
 * `struct sk_buff'
 * Eddie Kohler, Robert Morris, Nickolai Zeldovich
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2008-2011 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#define CLICK_PACKET_DEPRECATED_ENUM
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <click/sync.hh>
# include <click/tcpanno.hh>
# include <clicknet/ether.h>
# include <clicknet/ip.h>
# include <clicknet/ip6.h>
# include <clicknet/tcp.h>
# include <clicknet/udp.h>
# include <clicknet/tcp.hh>
#if CLICK_USERLEVEL || CLICK_MINIOS
# include <unistd.h>
# if HAVE_DPDK_PACKET
#  include <rte_config.h>
#  include <rte_common.h>
#  include <rte_memcpy.h>
#  include <rte_mbuf.h>
#  include <rte_mempool.h>
#  include <rte_ethdev.h>
#  include <rte_version.h>
# endif /* HAVE_DPDK_PACKET */
#endif
CLICK_DECLS

/** @file packet.hh
 * @brief The Packet class models packets in Click.
 */

/** @class Packet
 * @brief A network packet.
 * @nosubgrouping
 *
 * Click's Packet class represents network packets within a router.  Packet
 * objects are passed from Element to Element via the Element::push() and
 * Element::pull() functions.  The vast majority of elements handle packets.
 *
 * A packet consists of a <em>data buffer</em>, which stores the actual packet
 * wire data, and a set of <em>annotations</em>, which store extra information
 * calculated about the packet, such as the destination address to be used for
 * routing.  Every Packet object has different annotations, but a data buffer
 * may be shared among multiple Packet objects, saving memory and speeding up
 * packet copies.  (See Packet::clone.)  As a result a Packet's data buffer is
 * not writable.  To write into a packet, turn it into a nonshared
 * WritablePacket first, using uniqueify(), push(), or put().
 *
 * <h3>Data Buffer</h3>
 *
 * A packet's data buffer is a single flat array of bytes.  The buffer may be
 * larger than the actual packet data, leaving unused spaces called
 * <em>headroom</em> and <em>tailroom</em> before and after the data proper.
 * Prepending headers or appending data to a packet can be quite efficient if
 * there is enough headroom or tailroom.
 *
 * The relationships among a Packet object's data buffer variables is shown
 * here:
 *
 * <pre>
 *                     data()               end_data()
 *                        |                      |
 *       |<- headroom() ->|<----- length() ----->|<- tailroom() ->|
 *       |                v                      v                |
 *       +================+======================+================+
 *       |XXXXXXXXXXXXXXXX|   PACKET CONTENTS    |XXXXXXXXXXXXXXXX|
 *       +================+======================+================+
 *       ^                                                        ^
 *       |<------------------ buffer_length() ------------------->|
 *       |                                                        |
 *    buffer()                                               end_buffer()
 * </pre>
 *
 * Most code that manipulates packets is interested only in data() and
 * length().
 *
 * To create a Packet, call one of the make() functions.  To destroy a Packet,
 * call kill().  To clone a Packet, which creates a new Packet object that
 * shares this packet's data, call clone().  To uniqueify a Packet, which
 * unshares the packet data if necessary, call uniqueify().  To allocate extra
 * space for headers or trailers, call push() and put().  To remove headers or
 * trailers, call pull() and take().
 *
 * <pre>
 *                data()                          end_data()
 *                   |                                |
 *           push()  |  pull()                take()  |  put()
 *          <======= | =======>              <======= | =======>
 *                   v                                v
 *       +===========+================================+===========+
 *       |XXXXXXXXXXX|        PACKET CONTENTS         |XXXXXXXXXXX|
 *       +===========+================================+===========+
 * </pre>
 *
 * Packet objects are implemented in different ways in different drivers.  The
 * userlevel driver has its own C++ implementation.  In the linuxmodule
 * driver, however, Packet is an overlay on Linux's native sk_buff
 * object: the Packet methods access underlying sk_buff data directly, with no
 * overhead.  (For example, Packet::data() returns the sk_buff's data field.)
 *
 * <h3>Annotations</h3>
 *
 * Annotations are extra information about a packet above and beyond the
 * packet data.  Packet supports several specific annotations, plus a <em>user
 * annotation area</em> available for arbitrary use by elements.
 *
 * <ul>
 * <li><b>Header pointers:</b> Each packet has three header pointers, designed
 * to point to the packet's MAC header, network header, and transport header,
 * respectively.  Convenience functions like ip_header() access these pointers
 * cast to common header types.  The header pointers are kept up to date when
 * operations like push() or uniqueify() change the packet's data buffer.
 * Header pointers can be null, and they can even point to memory outside the
 * current packet data bounds.  For example, a MAC header pointer will remain
 * set even after pull() is used to shift the packet data past the MAC header.
 * As a result, functions like mac_header_offset() can return negative
 * numbers.</li>
 * <li><b>Timestamp:</b> A timestamp associated with the packet.  Most packet
 * sources timestamp packets when they enter the router; other elements
 * examine or modify the timestamp.</li>
 * <li><b>Device:</b> A pointer to the device on which the packet arrived.
 * Only meaningful in the linuxmodule driver, but provided in every
 * driver.</li>
 * <li><b>Packet type:</b> A small integer indicating whether the packet is
 * meant for this host, broadcast, multicast, or some other purpose.  Several
 * elements manipulate this annotation; in linuxmodule, setting the annotation
 * is required for the host network stack to process incoming packets
 * correctly.</li>
 * <li><b>Performance counter</b> (linuxmodule only): A 64-bit integer
 * intended to hold a performance counter value.  Used by SetCycleCount and
 * others.</li>
 * <li><b>Next and previous packet:</b> Pointers provided to allow elements to
 * chain packets into a doubly linked list.</li>
 * <li><b>Annotations:</b> Each packet has @link Packet::anno_size anno_size
 * @endlink bytes available for annotations.  Elements agree to use portions
 * of the annotation area to communicate per-packet information.  Macros in
 * the <click/packet_anno.hh> header file define the annotations used by
 * Click's current elements.  One common annotation is the network address
 * annotation -- see Packet::dst_ip_anno().  Routing elements, such as
 * RadixIPLookup, set the address annotation to indicate the desired next hop;
 * ARPQuerier uses this annotation to query the next hop's MAC.</li>
 * </ul>
 *
 * New packets start wth all annotations set to zero or null.  Cloning a
 * packet copies its annotations.
 */

/** @class WritablePacket
 * @brief A network packet believed not to be shared.
 *
 * The WritablePacket type represents Packet objects whose data buffers are
 * not shared.  As a result, WritablePacket's versions of functions that
 * access the packet data buffer, such as data(), end_buffer(), and
 * ip_header(), return mutable pointers (<tt>char *</tt> rather than <tt>const
 * char *</tt>).
 *
 * WritablePacket objects are created by Packet::make(), Packet::uniqueify(),
 * Packet::push(), and Packet::put(), which ensure that the returned packet
 * does not share its data buffer.
 *
 * WritablePacket's interface is the same as Packet's except for these type
 * differences.  For documentation, see Packet.
 *
 * @warning The WritablePacket convention reduces the likelihood of error
 * when modifying packet data, but does not eliminate it.  For instance, by
 * calling WritablePacket::clone(), it is possible to create a WritablePacket
 * whose data is shared:
 * @code
 * Packet *p = ...;
 * if (WritablePacket *q = p->uniqueify()) {
 *     Packet *p2 = q->clone();
 *     assert(p2);
 *     q->ip_header()->ip_v = 6;   // modifies p2's data as well
 * }
 * @endcode
 * Avoid writing buggy code like this!  Use WritablePacket selectively, and
 * try to avoid calling WritablePacket::clone() when possible. */

Packet::~Packet()
{
    // This is a convenient place to put static assertions.
    static_assert(addr_anno_offset % 8 == 0 && user_anno_offset % 8 == 0,
		  "Annotations must begin at multiples of 8 bytes.");
    static_assert(addr_anno_offset + addr_anno_size <= anno_size,
		  "Annotation area too small for address annotations.");
    static_assert(user_anno_offset + user_anno_size <= anno_size,
		  "Annotation area too small for user annotations.");
    static_assert(dst_ip_anno_offset == DST_IP_ANNO_OFFSET
		  && dst_ip6_anno_offset == DST_IP6_ANNO_OFFSET
		  && dst_ip_anno_size == DST_IP_ANNO_SIZE
		  && dst_ip6_anno_size == DST_IP6_ANNO_SIZE
		  && dst_ip_anno_size == 4
		  && dst_ip6_anno_size == 16
		  && dst_ip_anno_offset + 4 <= anno_size
		  && dst_ip6_anno_offset + 16 <= anno_size,
		  "Address annotations at unexpected locations.");
    static_assert((default_headroom & 3) == 0,
		  "Default headroom should be a multiple of 4 bytes.");
#if CLICK_LINUXMODULE
    static_assert(sizeof(Anno) <= sizeof(((struct sk_buff *)0)->cb),
		  "Anno structure too big for Linux packet annotation area.");
#endif

#if CLICK_LINUXMODULE
    panic("Packet destructor");
#elif !HAVE_DPDK_PACKET
    if (_data_packet)
	_data_packet->kill();
# if CLICK_USERLEVEL || CLICK_MINIOS
    else if (_head && _destructor)
	_destructor(_head, _end - _head, _destructor_argument);
    else
	delete[] _head;
# elif CLICK_BSDMODULE
    if (_m)
	m_freem(_m);
# endif
    _head = _data = 0;
#endif
}

#if !CLICK_LINUXMODULE && !HAVE_DPDK_PACKET

# if HAVE_CLICK_PACKET_POOL
// ** Packet pools **

// Click configurations usually allocate & free tons of packets and it's
// important to do so quickly. This specialized packet allocator saves
// pre-initialized Packet objects, either with or without data, for fast
// reuse. It can support multithreaded deployments: each thread has its own
// pool, with a global pool to even out imbalance.

#  define CLICK_PACKET_POOL_BUFSIZ		2048
#  define CLICK_PACKET_POOL_SIZE		1000 // see LIMIT in packetpool-01.testie
#  define CLICK_GLOBAL_PACKET_POOL_COUNT	16

namespace {
struct PacketData {
    PacketData* next;           // link to next free data buffer in pool
#  if HAVE_MULTITHREAD
    PacketData* batch_next;     // link to next buffer batch
    unsigned batch_pdcount;     // # buffers in this batch
#  endif
};

struct PacketPool {
    WritablePacket* p;          // free packets, linked by p->next()
    unsigned pcount;            // # packets in `p` list
    PacketData* pd;             // free data buffers, linked by pd->next
    unsigned pdcount;           // # buffers in `pd` list
#  if HAVE_MULTITHREAD
    PacketPool* thread_pool_next; // link to next per-thread pool
#  endif
};
}

#  if HAVE_MULTITHREAD
static __thread PacketPool *thread_packet_pool;

struct GlobalPacketPool {
    WritablePacket* pbatch;     // batches of free packets, linked by p->prev()
                                //   p->anno_u32(0) is # packets in batch
    unsigned pbatchcount;       // # batches in `pbatch` list
    PacketData* pdbatch;        // batches of free data buffers
    unsigned pdbatchcount;      // # batches in `pdbatch` list

    PacketPool* thread_pools;   // all thread packet pools
    volatile uint32_t lock;
};
static GlobalPacketPool global_packet_pool;
#else
static PacketPool global_packet_pool;
#  endif

/** @brief Return the local packet pool for this thread.
    @pre make_local_packet_pool() has succeeded on this thread. */
static inline PacketPool& local_packet_pool() {
#  if HAVE_MULTITHREAD
    return *thread_packet_pool;
#  else
    // If not multithreaded, there is only one packet pool.
    return global_packet_pool;
#  endif
}

/** @brief Create and return a local packet pool for this thread. */
static inline PacketPool* make_local_packet_pool() {
#  if HAVE_MULTITHREAD
    PacketPool *pp = thread_packet_pool;
    if (!pp && (pp = new PacketPool)) {
	memset(pp, 0, sizeof(PacketPool));
	while (atomic_uint32_t::swap(global_packet_pool.lock, 1) == 1)
	    /* do nothing */;
	pp->thread_pool_next = global_packet_pool.thread_pools;
	global_packet_pool.thread_pools = pp;
	thread_packet_pool = pp;
	click_compiler_fence();
	global_packet_pool.lock = 0;
    }
    return pp;
#  else
    return &global_packet_pool;
#  endif
}

WritablePacket *
WritablePacket::pool_allocate(bool with_data)
{
    PacketPool& packet_pool = *make_local_packet_pool();
    (void) with_data;

#  if HAVE_MULTITHREAD
    // Steal packets and/or data from the global pool if there's nothing on
    // the local pool.
    if ((!packet_pool.p && global_packet_pool.pbatch)
	|| (with_data && !packet_pool.pd && global_packet_pool.pdbatch)) {
	while (atomic_uint32_t::swap(global_packet_pool.lock, 1) == 1)
	    /* do nothing */;

	WritablePacket *pp;
	if (!packet_pool.p && (pp = global_packet_pool.pbatch)) {
	    global_packet_pool.pbatch = static_cast<WritablePacket *>(pp->prev());
	    --global_packet_pool.pbatchcount;
	    packet_pool.p = pp;
	    packet_pool.pcount = pp->anno_u32(0);
	}

	PacketData *pd;
	if (with_data && !packet_pool.pd && (pd = global_packet_pool.pdbatch)) {
	    global_packet_pool.pdbatch = pd->batch_next;
	    --global_packet_pool.pdbatchcount;
	    packet_pool.pd = pd;
	    packet_pool.pdcount = pd->batch_pdcount;
	}

	click_compiler_fence();
	global_packet_pool.lock = 0;
    }
#  endif /* HAVE_MULTITHREAD */

    WritablePacket *p = packet_pool.p;
    if (p) {
	packet_pool.p = static_cast<WritablePacket*>(p->next());
	--packet_pool.pcount;
    } else
	p = new WritablePacket;

    return p;
}

WritablePacket *
WritablePacket::pool_allocate(uint32_t headroom, uint32_t length,
			      uint32_t tailroom)
{
    uint32_t n = headroom + length + tailroom;
    if (n < CLICK_PACKET_POOL_BUFSIZ)
	n = CLICK_PACKET_POOL_BUFSIZ;
    WritablePacket *p = pool_allocate(n == CLICK_PACKET_POOL_BUFSIZ);
    if (p) {
	p->initialize();
	PacketData *pd;
	PacketPool& packet_pool = local_packet_pool();
	if (n == CLICK_PACKET_POOL_BUFSIZ && (pd = packet_pool.pd)) {
	    packet_pool.pd = pd->next;
	    --packet_pool.pdcount;
	    p->_head = reinterpret_cast<unsigned char *>(pd);
	} else if ((p->_head = new unsigned char[n]))
	    /* OK */;
	else {
	    delete p;
	    return 0;
	}
	p->_data = p->_head + headroom;
	p->_tail = p->_data + length;
	p->_end = p->_head + n;
    }
    return p;
}

void
WritablePacket::recycle(WritablePacket *p)
{
    unsigned char *data = 0;
    if (!p->_data_packet && p->_head && !p->_destructor
	&& p->_end - p->_head == CLICK_PACKET_POOL_BUFSIZ) {
	data = p->_head;
	p->_head = 0;
    }
    p->~WritablePacket();

    PacketPool& packet_pool = *make_local_packet_pool();
#  if HAVE_MULTITHREAD
    if ((packet_pool.p && packet_pool.pcount == CLICK_PACKET_POOL_SIZE)
	|| (data && packet_pool.pd && packet_pool.pdcount == CLICK_PACKET_POOL_SIZE)) {
	while (atomic_uint32_t::swap(global_packet_pool.lock, 1) == 1)
	    /* do nothing */;

	if (packet_pool.p && packet_pool.pcount == CLICK_PACKET_POOL_SIZE) {
	    if (global_packet_pool.pbatchcount == CLICK_GLOBAL_PACKET_POOL_COUNT) {
		while (WritablePacket *p = packet_pool.p) {
		    packet_pool.p = static_cast<WritablePacket *>(p->next());
		    ::operator delete((void *) p);
		}
	    } else {
		packet_pool.p->set_prev(global_packet_pool.pbatch);
                packet_pool.p->set_anno_u32(0, packet_pool.pcount);
		global_packet_pool.pbatch = packet_pool.p;
		++global_packet_pool.pbatchcount;
		packet_pool.p = 0;
	    }
	    packet_pool.pcount = 0;
	}

	if (data && packet_pool.pd && packet_pool.pdcount == CLICK_PACKET_POOL_SIZE) {
	    if (global_packet_pool.pdbatchcount == CLICK_GLOBAL_PACKET_POOL_COUNT) {
		while (PacketData *pd = packet_pool.pd) {
		    packet_pool.pd = pd->next;
		    delete[] reinterpret_cast<unsigned char *>(pd);
		}
	    } else {
		packet_pool.pd->batch_next = global_packet_pool.pdbatch;
                packet_pool.pd->batch_pdcount = packet_pool.pdcount;
		global_packet_pool.pdbatch = packet_pool.pd;
		++global_packet_pool.pdbatchcount;
		packet_pool.pd = 0;
	    }
	    packet_pool.pdcount = 0;
	}

	click_compiler_fence();
	global_packet_pool.lock = 0;
    }
#  else /* !HAVE_MULTITHREAD */
    if (packet_pool.pcount == CLICK_PACKET_POOL_SIZE) {
	::operator delete((void *) p);
	p = 0;
    }
    if (data && packet_pool.pdcount == CLICK_PACKET_POOL_SIZE) {
	delete[] data;
	data = 0;
    }
#  endif /* HAVE_MULTITHREAD */

    if (p) {
	++packet_pool.pcount;
	p->set_next(packet_pool.p);
	packet_pool.p = p;
	assert(packet_pool.pcount <= CLICK_PACKET_POOL_SIZE);
    }
    if (data) {
	++packet_pool.pdcount;
	PacketData *pd = reinterpret_cast<PacketData *>(data);
	pd->next = packet_pool.pd;
	packet_pool.pd = pd;
	assert(packet_pool.pdcount <= CLICK_PACKET_POOL_SIZE);
    }
}

# endif /* HAVE_PACKET_POOL */

bool
Packet::alloc_data(uint32_t headroom, uint32_t length, uint32_t tailroom)
{
    uint32_t n = length + headroom + tailroom;
    if (n < min_buffer_length) {
	tailroom = min_buffer_length - length - headroom;
	n = min_buffer_length;
    }
# if CLICK_USERLEVEL || CLICK_MINIOS
    unsigned char *d = new unsigned char[n];
    if (!d)
	return false;
    _head = d;
    _data = d + headroom;
    _tail = _data + length;
    _end = _head + n;
# elif CLICK_BSDMODULE
    //click_chatter("allocate new mbuf, length=%d", n);
    if (n > MJUM16BYTES) {
	click_chatter("trying to allocate %d bytes: too many\n", n);
	return false;
    }
    struct mbuf *m;
    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (!m)
	return false;
    if (n > MHLEN) {
	if (n > MCLBYTES)
	    m_cljget(m, M_DONTWAIT, (n <= MJUMPAGESIZE ? MJUMPAGESIZE :
				     n <= MJUM9BYTES   ? MJUM9BYTES   :
 							 MJUM16BYTES));
	else
	    MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
	    m_freem(m);
	    return false;
	}
    }
    _m = m;
    _m->m_data += headroom;
    _m->m_len = length;
    _m->m_pkthdr.len = length;
    assimilate_mbuf();
# endif /* CLICK_USERLEVEL || CLICK_BSDMODULE */
    return true;
}

#endif /* !CLICK_LINUXMODULE && !HAVE_DPDK_PACKET */


/** @brief Create and return a new packet.
 * @param headroom headroom in new packet
 * @param data data to be copied into the new packet
 * @param length length of packet
 * @param tailroom tailroom in new packet
 * @return new packet, or null if no packet could be created
 *
 * The @a data is copied into the new packet.  If @a data is null, the
 * packet's data is left uninitialized.  The resulting packet's
 * buffer_length() will be at least @link Packet::min_buffer_length
 * min_buffer_length @endlink; if @a headroom + @a length + @a tailroom would
 * be less, then @a tailroom is increased to make the total @link
 * Packet::min_buffer_length min_buffer_length @endlink.
 *
 * The new packet's annotations are cleared and its header pointers are
 * null. */
WritablePacket *
Packet::make(uint32_t headroom, const void *data,
	     uint32_t length, uint32_t tailroom, bool clear_annotations)
{
#if CLICK_LINUXMODULE
    int want = 1;
    if (struct sk_buff *skb = skbmgr_allocate_skbs(headroom, length + tailroom, &want)) {
	assert(want == 1);
	// packet comes back from skbmgr with headroom reserved
	__skb_put(skb, length);	// leave space for data
	if (data)
	    memcpy(skb->data, data, length);
# if PACKET_CLEAN
	skb->pkt_type = HOST | PACKET_CLEAN;
# else
	skb->pkt_type = HOST;
# endif
	WritablePacket *q = reinterpret_cast<WritablePacket *>(skb);
	if (clear_annotations)
	    q->clear_annotations();
	return q;
    } else
	return 0;
#elif HAVE_DPDK_PACKET
//     int s = rte_socket_id();
    int s = rte_lcore_index(rte_lcore_id());

    // Check size
    if (headroom + length + tailroom > rte_pktmbuf_data_room_size(mempool[s])) {
	click_chatter("requested DPDK packet size %u too big (max %u)",
	  headroom + length + tailroom, rte_pktmbuf_data_room_size(mempool[s]));
	return NULL;
    }

    // Allocate a buffer
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mempool[s]);
    if (!mbuf) {
	click_chatter("failed to allocate DPDK mbuf");
	return NULL;
    }

    // Check headroom and tailroom
    assert(headroom <= rte_pktmbuf_headroom(mbuf));
    assert(length + tailroom <= rte_pktmbuf_tailroom(mbuf));

    // Copy data
    char *d = rte_pktmbuf_append(mbuf, length);
    if (data && length > 0)
	rte_memcpy(d, data, length);

    // Create packet with annotations cleared (done in rte_pktmbuf_alloc)
    WritablePacket *p = Packet::make(mbuf, clear_annotations);

    return p;
# else
#  if HAVE_CLICK_PACKET_POOL
    WritablePacket *p = WritablePacket::pool_allocate(headroom, length, tailroom);
    if (!p)
	return 0;
#  else
    WritablePacket *p = new WritablePacket;
    if (!p)
	return 0;
    p->initialize();
    if (!p->alloc_data(headroom, length, tailroom)) {
	p->_head = 0;
	delete p;
	return 0;
    }
#  endif
    if (data)
	memcpy(p->data(), data, length);
    return p;
#endif
}

#if CLICK_USERLEVEL || CLICK_MINIOS
/** @brief Create and return a new packet (userlevel).
 * @param data data used in the new packet
 * @param length length of packet
 * @param destructor destructor function
 * @param argument argument to destructor function
 * @param headroom headroom available before the data pointer
 * @param tailroom tailroom available after data + length
 * @return new packet, or null if no packet could be created
 *
 * The packet's data pointer becomes the @a data: the data is not copied
 * into the new packet, rather the packet owns the @a data pointer. When the
 * packet's data is eventually destroyed, either because the packet is
 * deleted or because of something like a push() or full(), the @a
 * destructor will be called as @a destructor(@a data, @a length, @a
 * argument). (If @a destructor is null, the packet data will be freed by
 * <tt>delete[] @a data</tt>.) The packet has zero headroom and tailroom.
 *
 * The returned packet's annotations are cleared and its header pointers are
 * null. */
WritablePacket *
Packet::make(unsigned char *data, uint32_t length,
	     buffer_destructor_type destructor, void* argument, int headroom, int tailroom)
{
# if HAVE_DPDK_PACKET
    (void)data;
    (void)length;
    (void)destructor;
    (void)argument;
    (void)headroom;
    (void)tailroom;
    assert(0);
    return NULL;
# else
#  if HAVE_CLICK_PACKET_POOL
    WritablePacket *p = WritablePacket::pool_allocate(false);
#  else
    WritablePacket *p = new WritablePacket;
#  endif
    if (p) {
	p->initialize();
	p->_head = data - headroom;
	p->_data = data;
	p->_tail = data + length;
	p->_end = p->_tail + tailroom;
	p->_destructor = destructor;
        p->_destructor_argument = argument;
    }
    return p;
# endif /* HAVE_DPDK_PACKET */
}

/** @brief Copy the content and annotations of another packet (userlevel).
 * @param source packet
 * @param headroom for the new packet
 */
bool
Packet::copy(Packet* p, int headroom)
{
    if (headroom + p->length() > buffer_length())
        return false;
# if HAVE_DPDK_PACKET
    struct rte_mbuf *m = mbuf();
    struct rte_mbuf *n = p->mbuf();

    // Limted to one segment for now
    if (m->nb_segs != 1 || n->nb_segs != 1)
	return false;

    m->data_off = n->data_off;
    m->data_len = n->data_len;
    m->pkt_len  = n->pkt_len;
    rte_memcpy(m->buf_addr, n->buf_addr, n->buf_len);
# else
    _data = _head + headroom;
    memcpy(_data,p->data(),p->length());
    _tail = _data + p->length();
# endif /* HAVE_DPDK_PACKET */
    copy_annotations(p);
    set_mac_header(p->mac_header() ? data() + p->mac_header_offset() : 0);
    set_network_header(p->network_header() ? data() + p->network_header_offset() : 0, p->network_header_length());
    return true;
}
#endif


//
// UNIQUEIFICATION
//

/** @brief Create a clone of this packet.
 * @return the cloned packet
 *
 * The returned clone has independent annotations, initially copied from this
 * packet, but shares this packet's data.  shared() returns true for both the
 * packet and its clone.  Returns null if there's no memory for the clone. */
Packet *
Packet::clone()
{
#if CLICK_LINUXMODULE

    struct sk_buff *nskb = skb_clone(skb(), GFP_ATOMIC);
    return reinterpret_cast<Packet *>(nskb);

#elif HAVE_DPDK_PACKET
//     struct rte_mbuf *mi = rte_pktmbuf_clone(mbuf(), mempool[rte_socket_id()]);
    rte_prefetch0(aanno());
    rte_prefetch0((char *)anno() + CLICK_CACHE_LINE_SIZE);

    int s = rte_lcore_index(rte_lcore_id());
    struct rte_mbuf *mi = rte_pktmbuf_clone(mbuf(), mempool[s]);
    if (!mi) {
	click_chatter("Failed to clone DPDK packet. Obj %d/%d (available/in use). CLONED %d", rte_mempool_avail_count(mempool[s]), rte_mempool_in_use_count(mempool[s]), mbuf()->ol_flags & IND_ATTACHED_MBUF);
	return NULL;
    }
    Packet *p = reinterpret_cast<Packet *>(mi);

//    rte_prefetch0(aanno());
//    rte_prefetch0((char *)anno() + CLICK_CACHE_LINE_SIZE);
//    rte_prefetch0(p->aanno());
//    rte_prefetch0((char *)p->anno() + CLICK_CACHE_LINE_SIZE);

    rte_memcpy(p->aanno(), aanno(), sizeof(AllAnno));
    return p;
#elif CLICK_USERLEVEL || CLICK_BSDMODULE || CLICK_MINIOS
# if CLICK_BSDMODULE
    struct mbuf *m;

    if (this->_m == NULL)
        return 0;

    if (this->_m->m_flags & M_EXT
        && (   this->_m->m_ext.ext_type == EXT_JUMBOP
            || this->_m->m_ext.ext_type == EXT_JUMBO9
            || this->_m->m_ext.ext_type == EXT_JUMBO16)) {
        if ((m = dup_jumbo_m(this->_m)) == NULL)
	    return 0;
    }
    else if ((m = m_dup(this->_m, M_DONTWAIT)) == NULL)
	return 0;
# endif

    // timing: .31-.39 normal, .43-.55 two allocs, .55-.58 two memcpys
# if HAVE_CLICK_PACKET_POOL
    Packet *p = WritablePacket::pool_allocate(false);
# else
    Packet *p = new WritablePacket; // no initialization
# endif
    if (!p)
	return 0;
    Packet* origin = this;
    if (origin->_data_packet)
        origin = origin->_data_packet;
    memcpy(p, this, sizeof(Packet));
    p->_use_count = 1;
    p->_data_packet = origin;
# if CLICK_USERLEVEL || CLICK_MINIOS
    p->_destructor = 0;
# else
    p->_m = m;
# endif
    // increment our reference count because of _data_packet reference
    origin->_use_count++;
    return p;

#endif /* CLICK_LINUXMODULE */
}

WritablePacket *
Packet::expensive_uniqueify(int32_t extra_headroom, int32_t extra_tailroom,
			    bool free_on_failure)
{
    assert(extra_headroom >= (int32_t)(-headroom()) && extra_tailroom >= (int32_t)(-tailroom()));

#if CLICK_LINUXMODULE

    struct sk_buff *nskb = skb();

    // preserve this, which otherwise loses a ref here
    if (!free_on_failure)
        if (!(nskb = skb_clone(nskb, GFP_ATOMIC)))
            return NULL;

    // nskb is now not shared, which psk_expand_head asserts
    if (!(nskb = skb_share_check(nskb, GFP_ATOMIC)))
        return NULL;

    if (pskb_expand_head(nskb, extra_headroom, extra_tailroom, GFP_ATOMIC)) {
        kfree_skb(nskb);
        return NULL;
    }

    // success, so kill the clone from above
    if (!free_on_failure)
        kill();

    return reinterpret_cast<WritablePacket *>(nskb);

#elif HAVE_DPDK_PACKET
    // We can never add space to the buffer since its size is fixed
//    if (extra_headroom + extra_tailroom != 0) {
    if (extra_headroom || extra_tailroom) { // both must be zero for now
	click_chatter("failed to uniqueify DPDK packet");
	return NULL;
    }

    // If any segment was cloned, then make a clone and uniqueify that
    for (struct rte_mbuf *m = mbuf(); m; m = m->next) {
	if (RTE_MBUF_DIRECT(m) && rte_mbuf_refcnt_read(m) > 1) {
	    Packet *p = clone();
	    WritablePacket *q = (p ? p->expensive_uniqueify(extra_headroom, extra_tailroom, true) : 0);
	    if (q || free_on_failure)
		kill();
	    return q;
	}
    }

    static int chatter = 0;
    if (chatter < 5) {
	click_chatter("expensive uniqueify");
	chatter++;
    }

    const unsigned char *old_head = buffer();

    // Absolute offset value
//    uint32_t absoff = RTE_MAX(extra_headroom, extra_tailroom);

    // Uniqueify mbuf chain
    for (struct rte_mbuf *m = mbuf(); m; m = m->next) {
	// At this point, each mbuf is either indirect or direct with refcnt = 1
	if (RTE_MBUF_DIRECT(m)) {
	    assert(rte_mbuf_refcnt_read(m) == 1);
	    continue;
	}

	// Save data length and offset
	uint16_t data_len = m->data_len;
	uint16_t data_off = m->data_off;

	// Get direct mbuf
	struct rte_mbuf *md = rte_mbuf_from_indirect(m);
	struct rte_mempool *mp = md->pool;

	// Get buffer address 
	uint32_t mbuf_size = sizeof(struct rte_mbuf) + rte_pktmbuf_priv_size(mp);
	void *buf_addr = (char *)m + mbuf_size;

	// Copy data
	assert(m->buf_len == md->buf_len);
	rte_memcpy(buf_addr, md->buf_addr, m->buf_len);

	// Detach indirect mbuf
	rte_pktmbuf_detach(m);

	// Set data length and offset
	m->data_len = data_len;
	m->data_off = data_off;

# if RTE_VERSION < RTE_VERSION_NUM(16,7,0,0)
	// Reduce refcnt and free direct mbuf, if needed
	if (rte_mbuf_refcnt_update(md, -1) == 0)
	    rte_mempool_put(md->pool, md);
# endif

	// Adjust first segment
//	if (mi == mbuf()) {
//	    mi->data_off += extra_headroom;
//	    mi->data_len -= extra_headroom;
//	}

	// Copy data
//	char *src = rte_mbuf_to_baddr(md) + RTE_MAX(extra_tailroom, 0);
//	char *dst = rte_mbuf_to_baddr(mi) + RTE_MAX(extra_headroom, 0);
//	uint32_t len = md->buf_len - absoff;
//	memmove(dst, src, len);

	// Get next segment
//	struct rte_mbuf *ni = mi->next;

	// Copy remaining data
//	if (ni && extra_headroom) {
//	    assert(RTE_MBUF_INDIRECT(ni));
//	    struct rte_mbuf *nd = rte_mbuf_from_indirect(ni);
//	    src = (extra_headroom > 0 ? src+len : rte_pktmbuf_mtod(nd, char *));
//	    dst = (extra_headroom > 0 ? rte_pktmbuf_mtod(ni, char *) : dst+len);
//	    len = absoff;
//	    memmove(dst, src, len);
//	}

	// Adjust last segment	
//	if (!ni)
//	    mi->data_len += extra_headroom;

	// Reduce refcnt and free direct mbuf, if needed
//	if (rte_mbuf_refcnt_update(md, -1) == 0)
//	    rte_mempool_put(md->pool, md);

	// Process next segment
//	mi = ni;
    }

    shift_header_annotations(old_head, extra_headroom); 
    return static_cast<WritablePacket *>(this);

/*
    // If someone else has cloned this packet, then we need to leave its data
    // pointers around. Make a clone and uniqueify that.
    if (rte_mbuf_refcnt_read(mbuf()) > 1) {
	Packet *p = clone();
	WritablePacket *q = (p ? p->expensive_uniqueify(extra_headroom, extra_tailroom, true) : 0);
	if (q || free_on_failure)
	    kill();
	return q;
    }

    assert(RTE_MBUF_INDIRECT(mbuf()));
    static int chatter = 0;
    if (chatter < 5) {
	click_chatter("expensive uniqueify");
	chatter++;
    }

    const unsigned char *old_head = buffer();

    // mbuf pointers
    struct rte_mbuf *mi = mbuf();
    struct rte_mbuf *md = rte_mbuf_from_indirect(mi);

    // detach indirect mbuf
    rte_pktmbuf_detach(mi);

    // set packet length
    rte_pktmbuf_pkt_len(mi)  = rte_pktmbuf_pkt_len(md);
    rte_pktmbuf_data_len(mi) = rte_pktmbuf_data_len(md);

    // set data offset (headroom)
    mi->data_off = md->data_off + extra_headroom;

    void *src = rte_mbuf_to_baddr(md) + RTE_MAX(extra_tailroom, 0);
    void *dst = rte_mbuf_to_baddr(mi) + RTE_MAX(extra_headroom, 0);
    uint32_t len = md->buf_len - RTE_MAX(extra_headroom, extra_tailroom);
    rte_memcpy(dst, src, len);

    // free old data
    if (rte_mbuf_refcnt_update(md, -1) == 0)
	rte_mempool_put(md->pool, md);

    shift_header_annotations(old_head, extra_headroom); 
    return static_cast<WritablePacket *>(this);*/
#else /* !CLICK_LINUXMODULE */

    // If someone else has cloned this packet, then we need to leave its data
    // pointers around. Make a clone and uniqueify that.
    if (_use_count > 1) {
	Packet *p = clone();
	WritablePacket *q = (p ? p->expensive_uniqueify(extra_headroom, extra_tailroom, true) : 0);
	if (q || free_on_failure)
	    kill();
	return q;
    }

    uint8_t *old_head = _head, *old_end = _end;
# if CLICK_BSDMODULE
    struct mbuf *old_m = _m;
# endif

    if (!alloc_data(headroom() + extra_headroom, length(), tailroom() + extra_tailroom)) {
	if (free_on_failure)
	    kill();
	return 0;
    }

    unsigned char *start_copy = old_head + (extra_headroom >= 0 ? 0 : -extra_headroom);
    unsigned char *end_copy = old_end + (extra_tailroom >= 0 ? 0 : extra_tailroom);

    memcpy(_head + (extra_headroom >= 0 ? extra_headroom : 0), start_copy, end_copy - start_copy);

    // free old data
    if (_data_packet)
	_data_packet->kill();
#  if CLICK_USERLEVEL || CLICK_MINIOS
    else if (_destructor)
	_destructor(old_head, old_end - old_head, _destructor_argument);
    else
	delete[] old_head;
    _destructor = 0;
#  elif CLICK_BSDMODULE
    m_freem(old_m); // alloc_data() created a new mbuf, so free the old one
#  endif

    _use_count = 1;
    _data_packet = 0;
    shift_header_annotations(old_head, extra_headroom);
    return static_cast<WritablePacket *>(this);

#endif /* CLICK_LINUXMODULE */
}



#ifdef CLICK_BSDMODULE		/* BSD kernel module */
struct mbuf *
Packet::steal_m()
{
  struct Packet *p;
  struct mbuf *m2;

  p = uniqueify();
  m2 = p->m();

  /* Clear the mbuf from the packet: otherwise kill will MFREE it */
  p->_m = 0;
  p->kill();
  return m2;
}

/*
 * Duplicate a packet by copying data from an mbuf chain to a new mbuf with a
 * jumbo cluster (i.e., contiguous storage).
 */
struct mbuf *
Packet::dup_jumbo_m(struct mbuf *m)
{
  int len = m->m_pkthdr.len;
  struct mbuf *new_m;

  if (len > MJUM16BYTES) {
    click_chatter("warning: cannot allocate jumbo cluster for %d bytes", len);
    return NULL;
  }

  new_m = m_getjcl(M_DONTWAIT, m->m_type, m->m_flags & M_COPYFLAGS,
                   (len <= MJUMPAGESIZE ? MJUMPAGESIZE :
                    len <= MJUM9BYTES   ? MJUM9BYTES   :
                                          MJUM16BYTES));
  if (!new_m) {
    click_chatter("warning: jumbo cluster mbuf allocation failed");
    return NULL;
  }

  m_copydata(m, 0, len, mtod(new_m, caddr_t));
  new_m->m_len = len;
  new_m->m_pkthdr.len = len;

  /* XXX: Only a subset of what m_dup_pkthdr() would copy: */
  new_m->m_pkthdr.rcvif = m->m_pkthdr.rcvif;
# if __FreeBSD_version >= 800000
  new_m->m_pkthdr.flowid = m->m_pkthdr.flowid;
# endif
  new_m->m_pkthdr.ether_vtag = m->m_pkthdr.ether_vtag;

  return new_m;
}
#endif /* CLICK_BSDMODULE */

//
// EXPENSIVE_PUSH, EXPENSIVE_PUT
//

/*
 * Prepend some empty space before a packet.
 * May kill this packet and return a new one.
 */
WritablePacket *
Packet::expensive_push(uint32_t nbytes)
{
  static int chatter = 0;
  if (headroom() < nbytes && chatter < 5) {
    click_chatter("expensive Packet::push; have %d wanted %d",
                  headroom(), nbytes);
    chatter++;
  }
#if HAVE_DPDK_PACKET
  assert(nbytes <= headroom());
  if (WritablePacket *q = expensive_uniqueify(0, 0, true)) {
#else
  if (WritablePacket *q = expensive_uniqueify((nbytes + 128) & ~3, 0, true)) {
#endif
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_push(q->skb(), nbytes);
#elif HAVE_DPDK_PACKET
    rte_pktmbuf_prepend(q->mbuf(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_data -= nbytes;
# ifdef CLICK_BSDMODULE
    q->m()->m_data -= nbytes;
    q->m()->m_len += nbytes;
    q->m()->m_pkthdr.len += nbytes;
# endif
#endif
    return q;
  } else
    return 0;
}

WritablePacket *
Packet::expensive_put(uint32_t nbytes)
{
  static int chatter = 0;
  if (tailroom() < nbytes && chatter < 5) {
    click_chatter("expensive Packet::put; have %d wanted %d",
                  tailroom(), nbytes);
    chatter++;
  }
#if HAVE_DPDK_PACKET
  assert(nbytes <= tailroom());
  if (WritablePacket *q = expensive_uniqueify(0, 0, true)) {
#else
  if (WritablePacket *q = expensive_uniqueify(0, nbytes + 128, true)) {
#endif
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_put(q->skb(), nbytes);
#elif HAVE_DPDK_PACKET
    rte_pktmbuf_append(q->mbuf(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_tail += nbytes;
# ifdef CLICK_BSDMODULE
    q->m()->m_len += nbytes;
    q->m()->m_pkthdr.len += nbytes;
# endif
#endif
    return q;
  } else
    return 0;
}

Packet *
Packet::shift_data(int offset, bool free_on_failure)
{
    if (offset == 0)
	return this;

    // Preserve mac_header, network_header, and transport_header.
    const unsigned char *dp = data();
    if (has_mac_header() && mac_header() >= buffer()
	&& mac_header() <= end_buffer() && mac_header() < dp)
	dp = mac_header();
    if (has_network_header() && network_header() >= buffer()
	&& network_header() <= end_buffer() && network_header() < dp)
	dp = network_header();
    if (has_transport_header() && transport_header() >= buffer()
	&& transport_header() <= end_buffer() && transport_header() < dp)
	dp = network_header();

#if HAVE_DPDK_PACKET
# define min(a, b) ((a) < (b) ? (a) : (b))
    // Make sure no mbufs need to be allocated
    assert(offset < 0 ? (dp - buffer()) >= (ptrdiff_t)(-offset)
        : (tailroom() >= (uint32_t)offset));

    // Make sure the first and the last mbufs are non-empty
    assert(offset < 0 ? seg_last()->mbuf()->data_len > (-offset)
        : mbuf()->data_len > (uint32_t)offset);

    if (!shared()) {
	// The idea here is to shift data across all segments without changing
	// the length of each intermediate segment, i.e., only the lengths of
	// the first (-offset) and the last (+offset) segments will change
	if (offset > 0) {
	    // Create a vector with all mbufs for easy access
	    Vector<struct rte_mbuf *> mbufs;
	    for (struct rte_mbuf *m = mbuf(); m; m = m->next)
		mbufs.push_back(m);

	    // Source and destination mbuf indexes
	    int sidx = mbufs.size() - 1;
	    int didx = mbufs.size() - 1;

	    // Source and destination mbufs
	    struct rte_mbuf *sm = mbufs[sidx];
	    struct rte_mbuf *dm = mbufs[didx];

	    // Source and destination offsets
	    int soff = sm->data_len;
	    int doff = soff + offset;

	    // Header length
	    int hlen = int(data() - dp);

	    // Data and headers length
	    int len = length() + hlen;

	    while (len > 0) {
		// Byte count
		int cnt = min(soff, doff);

		// Make sure we also copy the headers
		if (sidx == 0 && soff == cnt)
		    cnt += hlen;

		// Offset update
		soff -= cnt;
		doff -= cnt;

		// Data pointers
		uint8_t *src = rte_pktmbuf_mtod(sm, uint8_t *) + soff;
		uint8_t *dst = rte_pktmbuf_mtod(dm, uint8_t *) + doff;

		// Data copy
		memmove(dst, src, cnt);

		// Get previous segment
		if (soff == 0 && sidx > 0) {
		    sm = mbufs[--sidx];
		    soff = sm->data_len;
		}
		if (doff == 0 && didx > 0) {
		    dm = mbufs[--didx];
		    doff = dm->data_len;
		}

		// Reduce remaining length
		len -= cnt;
	    }

	    // Adjust data offset
	    rte_pktmbuf_adj(mbuf(), offset);
	    rte_pktmbuf_append(mbuf(), offset);
	}
	else {
	    // Source and destination mbufs
	    struct rte_mbuf *sm = mbuf();
	    struct rte_mbuf *dm = mbuf();

	    // Source and destination offsets
	    int soff = -int(data() - dp);
	    int doff = soff + offset;

	    // Header length
	    int hlen = int(data() - dp);

	    // Data and headers length 
	    int len = length() + hlen;

	    while (len > 0) {
		// Data pointers
		uint8_t *src = rte_pktmbuf_mtod(sm, uint8_t *) + soff;
		uint8_t *dst = rte_pktmbuf_mtod(dm, uint8_t *) + doff;

		// Byte count
		int cnt = min(sm->data_len - soff, dm->data_len - doff);

		// Data copy
		memmove(dst, src, cnt);

		// Offset update
		soff += cnt;
		doff += cnt;

		// Get next segment
		if (soff == sm->data_len) {
		    sm = sm->next;
		    soff = 0;
		}
		if (doff == dm->data_len) {
		    dm = dm->next;
		    doff = 0;
		}

		// Reduce remaining length
		len -= cnt;
	    }

	    // Adjust data offset
	    rte_pktmbuf_prepend(mbuf(), -offset);
	    rte_pktmbuf_trim(mbuf(), -offset);
	}

	shift_header_annotations(buffer(), offset);
	return this;
    }
    else {
	// Expensive uniqueify only supports zero offset for now
	WritablePacket *p = expensive_uniqueify(0, 0, free_on_failure);
	return (p ? p->shift_data(offset, free_on_failure) : NULL);
    }
# undef min
#else 
    if (!shared()
	&& (offset < 0 ? (dp - buffer()) >= (ptrdiff_t)(-offset)
	    : tailroom() >= (uint32_t)offset)) {
	WritablePacket *q = static_cast<WritablePacket *>(this);
	memmove((unsigned char *) dp + offset, dp, q->end_data() - dp);
# if CLICK_LINUXMODULE
	struct sk_buff *mskb = q->skb();
	mskb->data += offset;
	mskb->tail += offset;
# else				/* User-space and BSD kernel module */
	q->_data += offset;
	q->_tail += offset;
#  if CLICK_BSDMODULE
	q->m()->m_data += offset;
#  endif
# endif
	shift_header_annotations(q->buffer(), offset);
	return this;
    } else {
	int tailroom_offset = (offset < 0 ? -offset : 0);
	if (offset < 0 && headroom() < (uint32_t)(-offset))
	    offset = -headroom() + ((uintptr_t)(data() + offset) & 7);
	else
	    offset += ((uintptr_t)buffer() & 7);
	return expensive_uniqueify(offset, tailroom_offset, free_on_failure);
    }
#endif /* HAVE_DPDK_PACKET */
}


#if HAVE_CLICK_PACKET_POOL
static void
cleanup_pool(PacketPool *pp, int global)
{
    unsigned pcount = 0, pdcount = 0;
    while (WritablePacket *p = pp->p) {
	++pcount;
	pp->p = static_cast<WritablePacket *>(p->next());
	::operator delete((void *) p);
    }
    while (PacketData *pd = pp->pd) {
	++pdcount;
	pp->pd = pd->next;
	delete[] reinterpret_cast<unsigned char *>(pd);
    }
    assert(pcount <= CLICK_PACKET_POOL_SIZE);
    assert(pdcount <= CLICK_PACKET_POOL_SIZE);
    assert(global || (pcount == pp->pcount && pdcount == pp->pdcount));
}
#endif

#if HAVE_DPDK
Packet::mempoolTable* Packet::mempool;


void 
Packet::destroy(unsigned char *, size_t, void *buf){
	rte_pktmbuf_free(static_cast<struct rte_mbuf *>(buf));
}

WritablePacket *
Packet::mbuf2packet(struct rte_mbuf *m)
{
# if HAVE_DPDK_PACKET
	WritablePacket *p = make(m);
# else
	// Get packet data and length
	uint8_t *d = (uint8_t *)rte_mbuf_to_baddr(m);
	uint16_t len = rte_pktmbuf_headroom(m) + \
	               rte_pktmbuf_data_len(m) + \
	               rte_pktmbuf_tailroom(m);

	// Create the packet with zero-copy and own destructor
	WritablePacket *p = make(d, len, destroy, m);

	// Adjust pointers for headroom and tailroom
	p->pull(rte_pktmbuf_headroom(m));
	p->take(rte_pktmbuf_tailroom(m));
# endif

	return p;
}

struct rte_mbuf * 
Packet::packet2mbuf(bool tx_ip_checksum, bool tx_tcp_checksum, bool tx_udp_checksum, bool tx_tcp_tso)
{
	struct rte_mbuf *m;
# if HAVE_DPDK_PACKET
	m = mbuf();
# else
// 	// Zero-copy operation if an unshared DPDK packet
// 	if (is_dpdk_packet(this) && !shared()) {
// 		m = (struct rte_mbuf *)destructor_argument();
// 		m->data_off = headroom();
// 		set_buffer_destructor(fake_destroy);
// 	}
// 	else {
	int s = rte_lcore_index(rte_lcore_id());
	m = rte_pktmbuf_alloc(mempool[s]);
	if (!m)
		return NULL;
	m->data_off = headroom();
	rte_memcpy(rte_pktmbuf_mtod(m, char *), data(), length());

	// Increase mbuf to match the packet length
	rte_pktmbuf_append(m, length());
# endif

	// Checksum and TCP segmentation offloading
	if (tx_ip_checksum  || tx_tcp_checksum || tx_udp_checksum) {
		// Ethernet header
		m->l2_len = sizeof(click_ether);
		click_ether *eh = rte_pktmbuf_mtod(m, click_ether *);
		uint16_t ether_type = ntohs(eh->ether_type);

		// VLAN header
		if (ether_type == ETHERTYPE_8021Q) {
			m->l2_len = sizeof(click_ether_vlan);
			click_ether_vlan *vh = (click_ether_vlan *)(eh + 1);
			ether_type = ntohs(vh->ether_vlan_encap_proto);
		}

		// IP checksum
		uint8_t proto = 0;
		if (ether_type == ETHERTYPE_IP) {
			click_ip *ip = (click_ip *)((char *)eh + m->l2_len);
			m->l3_len = ip->ip_hl << 2;
			m->ol_flags |= PKT_TX_IPV4;
			proto = ip->ip_p;
			if (tx_ip_checksum) {
				ip->ip_sum = 0;
				m->ol_flags |= PKT_TX_IP_CKSUM;
			}
		}
		else if (ether_type == ETHERTYPE_IP6) {
			click_ip6 *ip = (click_ip6 *)((char *)eh + m->l2_len);
			m->l3_len = sizeof(click_ip6);
			m->ol_flags |= PKT_TX_IPV6;
			proto = ip->ip6_nxt;
		}

		// TCP/UDP checksum
		char *l3_hdr = (char *)eh + m->l2_len;
		if (proto == IP_PROTO_UDP && tx_udp_checksum) {
			m->ol_flags |= PKT_TX_UDP_CKSUM;
			click_udp *uh = (click_udp *)(l3_hdr + m->l3_len);
			if (ether_type == ETHERTYPE_IP) {
				struct ipv4_hdr *ip = (struct ipv4_hdr *)l3_hdr;
				uh->uh_sum = rte_ipv4_phdr_cksum(ip, m->ol_flags);
			}
			else if (ether_type == ETHERTYPE_IP6) {
				struct ipv6_hdr *ip = (struct ipv6_hdr *)l3_hdr;
				uh->uh_sum = rte_ipv6_phdr_cksum(ip, m->ol_flags);
			}
		}
		else if (proto == IPPROTO_TCP && tx_tcp_checksum) {
			m->ol_flags |= PKT_TX_TCP_CKSUM;
			click_tcp *th = (click_tcp *)(l3_hdr + m->l3_len);
			m->l4_len = (th->th_off << 2);
			
			uint32_t data = m->pkt_len - m->l4_len - m->l3_len -m->l2_len;
			
			// TCP segmentation offloading (must be before cksum)
			// Silently disable TSO in case of SYN, RST and zero sized packet  
			if (tx_tcp_tso && TCP_MSS_ANNO(this) && !(TCP_SYN(th) || TCP_RST(th) || TCP_FIN(th) || data < 1)) {

				m->ol_flags |= PKT_TX_TCP_SEG;
				m->tso_segsz = TCP_MSS_ANNO(this);
			}

			if (ether_type == ETHERTYPE_IP) {
				struct ipv4_hdr *ip = (struct ipv4_hdr *)l3_hdr;
				th->th_sum = rte_ipv4_phdr_cksum(ip, m->ol_flags);
			}
			else if (ether_type == ETHERTYPE_IP6) {
				struct ipv6_hdr *ip = (struct ipv6_hdr *)l3_hdr;
				th->th_sum = rte_ipv6_phdr_cksum(ip, m->ol_flags);
			}
		}
	}

	return m;
}


void
Packet::static_initialize()
{
    unsigned lcore_id;
  
    // size reserved for the data
    uint16_t data_room_size = RTE_PKTMBUF_HEADROOM + RTE_MBUF_DEFAULT_DATAROOM + 9162;
    
    
    // Resize mempool
    mempool = new mempoolTable[rte_lcore_count()];
# if HAVE_DPDK_PACKET

    // Reserve space for click annotations
    uint16_t priv_data_size = RTE_ALIGN(sizeof(AllAnno), CLICK_CACHE_LINE_SIZE);
    uint32_t size = sizeof(struct rte_mbuf) + priv_data_size + data_room_size;

    // Private data
    struct rte_pktmbuf_pool_private priv;
    priv.mbuf_data_room_size = data_room_size;
    priv.mbuf_priv_size = priv_data_size;

    // Allocate a per-core mempool
    RTE_LCORE_FOREACH(lcore_id) {
	int t = rte_lcore_index(lcore_id);
	mempool[t] = rte_mempool_create(
	                (String("POOL_") + t).c_str(),           // name
	                64 * 1024 - 1,                          // pool size
	                size,                                    // element size
	                RTE_MEMPOOL_CACHE_MAX_SIZE,              // cache size
	                sizeof(struct rte_pktmbuf_pool_private), // priv size
	                rte_pktmbuf_pool_init,                   // mp_init
	                &priv,                                   // mp_init_arg
	                rte_pktmbuf_init,                        // obj_init
	                NULL,                                    // obj_init_arg
	                rte_lcore_to_socket_id(lcore_id),        // socket_id
	                MEMPOOL_F_NO_SPREAD);                    // flags


	if (!mempool[t])
	    rte_exit(EXIT_FAILURE, "failed to create mempool\n");
    }

    
# else
    // Allocate a per-core mempool
    RTE_LCORE_FOREACH(lcore_id) {
	int t = rte_lcore_index(lcore_id);
	mempool[t] = rte_pktmbuf_pool_create(
				    (String("POOL_") + t).c_str(),      // name
				    64 * 1024 - 1,                     // pool size
		                    RTE_MEMPOOL_CACHE_MAX_SIZE,         // cache size
		                    0,                                  // priv size
		                    data_room_size,                     // data size
		                    rte_lcore_to_socket_id(lcore_id));  // socket id
	if (!mempool[t])
	    rte_exit(EXIT_FAILURE, "failed to create mempool\n");
    }
# endif  /* HAVE_DPDK_PACKET */
}
#endif /* HAVE_DPDK */

void
Packet::static_cleanup()
{
#if HAVE_CLICK_PACKET_POOL
# if HAVE_MULTITHREAD
    while (PacketPool* pp = global_packet_pool.thread_pools) {
	global_packet_pool.thread_pools = pp->thread_pool_next;
	cleanup_pool(pp, 0);
	delete pp;
    }
    unsigned rounds = global_packet_pool.pbatchcount;
    if (rounds < global_packet_pool.pdbatchcount)
        rounds = global_packet_pool.pdbatchcount;
    assert(rounds <= CLICK_GLOBAL_PACKET_POOL_COUNT);
    PacketPool fake_pool;
    while (global_packet_pool.pbatch || global_packet_pool.pdbatch) {
        if ((fake_pool.p = global_packet_pool.pbatch))
            global_packet_pool.pbatch = static_cast<WritablePacket*>(fake_pool.p->prev());
        if ((fake_pool.pd = global_packet_pool.pdbatch))
            global_packet_pool.pdbatch = fake_pool.pd->batch_next;
	cleanup_pool(&fake_pool, 1);
	--rounds;
    }
    assert(rounds == 0);
# else
    cleanup_pool(&global_packet_pool, 0);
# endif
#endif
}

CLICK_ENDDECLS
