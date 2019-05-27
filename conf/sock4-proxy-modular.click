// -------------------------------------------------------------------
// |                   Vars and elems definitions                    |
// -------------------------------------------------------------------

require(library general-tcp.click)

define($DEV0 eth1, $ADDR0 10.0.0.1, $MAC0 aa:aa:aa:aa:aa:aa)
define($DEV1 eth2, $ADDR1 10.0.1.1, $MAC1 bb:bb:bb:bb:bb:bb)

AddressInfo($DEV0 $ADDR0 $MAC0);
AddressInfo($DEV1 $ADDR1 $MAC1);

dpdk0 :: DPDK($DEV0, BURST 32, TX_RING_SIZE 512, RX_RING_SIZE 512, TX_IP_CHECKSUM 1, TX_TCP_CHECKSUM 1, RX_CHECKSUM 1, RX_STRIP_CRC 1);
dpdk1 :: DPDK($DEV1, BURST 32, TX_RING_SIZE 512, RX_RING_SIZE 512, TX_IP_CHECKSUM 1, TX_TCP_CHECKSUM 1, RX_CHECKSUM 1, RX_STRIP_CRC 1);

tcp_layer :: TCPLayer(ADDRS $ADDR0 $ADDR1, VERBOSE 0, BUCKETS 131072);

tcp_epolls :: TCPEpollServer($DEV0, 9000, VERBOSE 1, PID 1);
tcp_epollc :: TCPEpollClient($DEV1, 9000, VERBOSE 1, PID 1);
tcp_proxy :: Socks4Proxy(VERBOSE 1, PID 1);
tcp_out :: Tee();

// -------------------------------------------------------------------
// |                         APP Wiring                              |
// -------------------------------------------------------------------


tcp_layer[1] -> tcp_out;

tcp_proxy[0] -> [1]tcp_epolls[1] -> [1]tcp_layer;
tcp_out[0] -> [0]tcp_epolls[0] -> [0]tcp_proxy;

tcp_proxy[1] -> [1]tcp_epollc[1] -> [1]tcp_layer;
tcp_out[1] -> [0]tcp_epollc[0] -> [1]tcp_proxy;

//NOTE tcp <-> app (or epollsrv/epollcli) connections are not used. 

// -------------------------------------------------------------------
// |                         ARP Protocol                            |
// ------------------------------------------------------------------- 

arpr0 :: ARPResponder($DEV0);
arpr0[0]
  -> dpdk0;

arpq0 :: ARPQuerier($DEV0, SHAREDPKT true, TIMEOUT 0, POLL_TIMEOUT 0);
arpq0[0]     // TCP/IP Packet
//  -> SetTCPChecksum(SHAREDPKT true)
//  -> SetIPChecksum(SHAREDPKT true)
  -> dpdk0;
arpq0[1]     // ARP Query
  -> dpdk0;

arpr1 :: ARPResponder($DEV1);
arpr1
  -> dpdk1;

arpq1 :: ARPQuerier($DEV1, SHAREDPKT true, TIMEOUT 0, POLL_TIMEOUT 0);
arpq1[0]     // TCP/IP Packet
//  -> SetTCPChecksum(SHAREDPKT true)
//  -> SetIPChecksum(SHAREDPKT true)
  -> dpdk1;
arpq1[1]     // ARP Query
  -> dpdk1;

// -------------------------------------------------------------------
// |                            RX packets                           |
// -------------------------------------------------------------------

dpdk0
  -> HostEtherFilter($DEV0)
  -> class0 :: FastClassifier(12/0800,         // IP - 1st out of FastClassifier may be send batches
                             12/0806 20/0002, // ARP response
                             12/0806 20/0001); // ARP query
     class0[2] -> [0]arpr0
              -> dpdk0;
     class0[1] -> [1]arpq0;
     class0[0] -> Strip(14)
              -> Print("RX0")
              -> CheckIPHeader(CHECKSUM false)
	      -> IPPrint("RX0")
              -> FastIPClassifier(tcp dst host $ADDR0)  // 1st out of FastIPClassifier may be send batches
              -> CheckTCPHeader(CHECKSUM false)
              -> [0]tcp_layer;


dpdk1
  -> HostEtherFilter($DEV1)
  -> class1 :: FastClassifier(12/0800,         // IP - 1st out of FastClassifier may be send batches
                             12/0806 20/0002, // ARP response
                             12/0806 20/0001); // ARP query
     class1[2] -> [0]arpr1
              -> dpdk1; 
     class1[1] -> [1]arpq1;
     class1[0] -> Strip(14)
              -> Print("RX1")
              -> CheckIPHeader(CHECKSUM false)
              -> IPPrint("RX1")
              -> FastIPClassifier(tcp dst host $ADDR1)  // 1st out of FastIPClassifier may be send batches
              -> CheckTCPHeader(CHECKSUM false)
              -> [0]tcp_layer;

// -------------------------------------------------------------------
// |                   TX packets (fake routing)                     |
// -------------------------------------------------------------------

tcp_layer[0]
  -> ic :: IPClassifier(tcp src host $DEV0, tcp src host $DEV1);

     ic[0]
       -> GetIPAddress(16)   // This only works with nodes in the same network
       -> IPPrint("TX0")
       -> [0]arpq0;

     ic[1]
       -> GetIPAddress(16)   // This only works with nodes in the same network
       -> IPPrint("TX1")
       -> [0]arpq1;


