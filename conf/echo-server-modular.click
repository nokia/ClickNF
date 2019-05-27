require(library general-tcp.click)

define($DEV0 iface, $ADDR0 10.0.20.2, $MAC0 bb:bb:bb:bb:bb:bb)
AddressInfo($DEV0 $ADDR0 $MAC0);

dpdk0 :: DPDK($DEV0, BURST 32, TX_RING_SIZE 512, RX_RING_SIZE 512, TX_IP_CHECKSUM 1, TX_TCP_CHECKSUM 1, RX_CHECKSUM 1, RX_STRIP_CRC 1);

tcp_layer :: TCPLayer(ADDRS $ADDR0, VERBOSE 0, BUCKETS 131072);
tcp_epoll :: TCPEpollServer($DEV0, 9000, BATCH 32, VERBOSE 0);
tcp_echos :: EchoServer(VERBOSE 0);

tcp_echos[0] -> [1]tcp_epoll[1] -> [1]tcp_layer;
tcp_layer[1] -> [0]tcp_epoll[0] -> [0]tcp_echos;

arpr :: ARPResponder($DEV0);
arpq :: ARPQuerier($DEV0, SHAREDPKT true, TIMEOUT 0, POLL_TIMEOUT 0);

arpq[0]     // Send TCP/IP Packet
//  -> SetTCPChecksum(SHAREDPKT true) // Enable in case of software Checksum
//  -> SetIPChecksum(SHAREDPKT true)  // Enable in case of software Checksum
  -> dpdk0;
arpq[1]     // Send ARP Query
  -> dpdk0;

tcp_layer[0]
  -> GetIPAddress(16) 
  -> [0]arpq;

dpdk0
  -> HostEtherFilter($DEV0)
  -> class :: FastClassifier(12/0800,         // IP - 1st out of FastClassifier may send batches
                             12/0806 20/0002, // ARP response
                             12/0806 20/0001); // ARP query
     class[2] -> [0]arpr
              -> dpdk0;
     class[1] -> [1]arpq;
     class[0] -> Strip(14)
              -> CheckIPHeader(CHECKSUM false)
              -> FastIPClassifier(tcp dst host $ADDR0)  // 1st out of FastIPClassifier may send batches
              -> CheckTCPHeader(CHECKSUM false)
              -> [0]tcp_layer;

