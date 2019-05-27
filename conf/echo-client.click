require(library general-tcp.click)

define($DEV0 iface, $ADDR0 10.0.20.1, $MAC0 aa:aa:aa:aa:aa:aa)
AddressInfo($DEV0 $ADDR0 $MAC0);

tcp_layer :: TCPLayer(ADDRS $ADDR0, VERBOSE 0);
tcp_echoc :: TCPEchoClientEpollZC(ADDRESS 10.0.20.2, PORT 9000, CONNECTIONS 500000, PARALLEL 64)

tcp_echoc[0] -> [1]tcp_layer;
tcp_layer[1] -> [0]tcp_echoc;

dpdk0 :: DPDK($DEV0, BURST 32, TX_RING_SIZE 512, RX_RING_SIZE 512, TX_IP_CHECKSUM 1, TX_TCP_CHECKSUM 1, RX_CHECKSUM 1, RX_STRIP_CRC 1, HASH_OFFLOAD 0);

arpq :: ARPQuerier($DEV0, SHAREDPKT true);
arpr :: ARPResponder($DEV0);

arpq[0]     // Send TCP/IP Packet
//  -> SetTCPChecksum(SHAREDPKT true) // Enable in case of software Checksum
//  -> SetIPChecksum(SHAREDPKT true)  // Enable in case of software Checksum
  -> dpdk0;
arpq[1]     // Send ARP Query
  -> dpdk0;

tcp_layer[0]
  -> GetIPAddress(16)  // This only works with nodes in the same network
  -> [0]arpq;


dpdk0 -> HostEtherFilter(iface)
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
