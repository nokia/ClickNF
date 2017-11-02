require(library test-tcp-layer2.click)

define($DEV0 iface, $ADDR0 10.0.20.2, $MAC0 bb:bb:bb:bb:bb:bb)
AddressInfo($DEV0 $ADDR0 $MAC0);

tcp_layer :: TCPLayer(ADDRS $ADDR0, VERBOSE false, BUCKETS 131072);
tcp_echos :: TCPEchoServerEpollZC($DEV0, 9000, VERBOSE false, BATCH 64);

tcp_echos[0] -> [1]tcp_layer;
tcp_layer[1] -> [0]tcp_echos;

dpdk0 :: DPDK($DEV0, BURST 32, TX_RING_SIZE 512, RX_RING_SIZE 512, TX_IP_CHECKSUM 1, TX_TCP_CHECKSUM 1, RX_CHECKSUM 1, RX_STRIP_CRC 1);

arpr :: ARPResponder($DEV0);
arpq :: ARPQuerier($DEV0, SHAREDPKT true, TIMEOUT 0, POLL_TIMEOUT 0);

arpq[0]     // Send TCP/IP Packet
//  -> SetTCPChecksum(SHAREDPKT true) // Enable in case of software Checksum
//  -> SetIPChecksum(SHAREDPKT true)  // Enable in case of software Checksum
  -> dpdk0;
arpq[1]     // Send ARP Query
  -> dpdk0;

tcp_layer[0]
  -> GetIPAddress(16)  // This only works with nodes in the same network
  -> [0]arpq;

dpdk0
  -> HostEtherFilter($DEV0)
  -> class :: FastClassifier(12/0806 20/0001, // ARP query
                             12/0806 20/0002, // ARP response
                             12/0800);        // IP
     class[0] -> [0]arpr
              -> dpdk0;
     class[1] -> [1]arpq;
     class[2] -> Strip(14)
              -> CheckIPHeader(CHECKSUM false)
              -> FastIPClassifier(tcp dst host $ADDR0)
              -> CheckTCPHeader(CHECKSUM false)
              -> [0]tcp_layer;
