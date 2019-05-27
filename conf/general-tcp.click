elementclass TCPLayer { __REST__ $rest |


	// General TCP info
	TCPInfo($rest);
	
	// Outgoing packets
	bbr_out :: TCPSetMssAnno
			-> BBRTCPTransmit
	        -> [0]output;  // To the network
	        
	tcp_out :: TCPSetMssAnno
	        -> [0]output;  // To the network
        

	// Pace packet transmission
	pace_out :: BBRTCPPacing
	         -> bbr_out;
	         
	classifier :: TCPClassifier -> tcp_out;
	classifier[1] -> tcp_out;
	classifier[2] -> pace_out;
	 
	// SYN
	snd_syn :: TCPSynOptionsEncap
	        -> TCPSynEncap
	        -> TCPIPEncap
	        -> TCPEnqueue4RTX
	        -> classifier;

	// ACK
	snd_ack :: TCPAckOptionsEncap
		    -> TCPAckEncap
		    -> TCPIPEncap
		    -> TCPEnqueue4RTX
	        -> classifier;

	// FIN
	snd_fin :: TCPAckOptionsEncap
	        -> TCPFinEncap
	        -> TCPIPEncap
	        -> TCPEnqueue4RTX
	        -> tcp_out;

	// RST
	snd_rst :: TCPAckOptionsEncap
	        -> TCPRstEncap
	        -> TCPIPEncap
	        -> TCPEnqueue4RTX
	        -> tcp_out;

	// Resetter
	snd_rtr :: TCPResetter
	        -> tcp_out;

	// Socket
	socket :: TCPSocket;
	socket[0] -> snd_syn;  // connect()
	socket[1] -> snd_rst;  // close()
	socket[2] -> snd_fin;  // close()
	socket[3] -> TCPNagle  // send() or push()
	          -> TCPRateControl
	          -> snd_ack;
	socket[4] -> [1]output;

	input[1] -> [0]socket;
	
	// Retransmissions (header replaced to update timestamp, SACK, WIN, ACK)
	classifier_rtx :: TCPClassifier -> tcp_out;
	classifier_rtx[1] -> tcp_out;
	classifier_rtx[2] -> bbr_out;
	
	snd_rtx :: TCPFlagDemux;
	snd_rtx[0] -> TCPReplacePacket     // SYN or SYN-ACK
	           -> SetTimestamp
	           -> TCPSynOptionsEncap
	           -> TCPSynEncap
	           -> TCPIPEncap
	           -> classifier_rtx;
	snd_rtx[1] -> TCPReplacePacket     // FIN or FIN-ACK
	           -> SetTimestamp
	           -> TCPAckOptionsEncap
	           -> TCPFinEncap
	           -> TCPIPEncap
	           -> classifier_rtx;
	snd_rtx[2] -> SetTimestamp        // ACK
	           -> StripIPHeader
	           -> TCPSetSeqAnno
	           -> StripTCPHeader
	           -> TCPAckOptionsEncap
	           -> TCPAckEncap
	           -> TCPGetSeqAnno
	           -> TCPIPEncap
	           -> classifier_rtx;

	// Timers
	timer :: TCPTimers(TICK 0.001);
	timer[0] -> TCPNewRenoRTX         // Retransmissions
	         -> snd_rtx;
	timer[1] -> TCPAckOptionsEncap    // Keepalive
	         -> TCPAckEncap
	         -> TCPIPEncap
	         -> DecTCPSeqNo
	         -> classifier;
	timer[2] -> snd_ack;              // Delayed ACK
	timer[3] -> classifier_rtx;		      // Paced packet out




	// Received packets
	input[0] 
	-> TCPFlowLookup
	-> dmx :: TCPStateDemux;
	   // CLOSED
	   dmx[0] -> TCPClosed -> snd_rtr;

	   // LISTEN
	   dmx[1] -> listen :: TCPListen;
	             listen[0] -> TCPSynOptionsParse             // Recv SYN
	                       -> TCPNewRenoSyn
	                       -> TCPReplacePacket
	                       -> snd_syn;                       // Send SYN-ACK

	             listen[1] -> snd_rtr;                       // Send RST

	   // SYN_SENT
	   dmx[2] -> synsent :: TCPSynSent;
	             synsent[0] -> TCPSynOptionsParse            // Recv SYN-ACK
	                        -> TCPEstimateRTT
	                        -> TCPNewRenoSyn
	                        -> TCPReplacePacket
	                        -> snd_ack;                      // Send ACK

	             synsent[1] -> TCPSynOptionsParse            // Recv SYN
	                        -> TCPNewRenoSyn
	                        -> TCPReplacePacket
	                        -> snd_syn;                      // Send SYN-ACK

	             synsent[2] -> snd_rtr;                      // Send RST

	   // Other states
	   dmx[3] -> optpars :: TCPAckOptionsParse  // Parse TCP options
	          -> estirtt :: TCPEstimateRTT      // Update RTT measurements
	          -> ckseqno :: TCPCheckSeqNo       // Ensure data is in window
	          -> trimpkt :: TCPTrimPacket       // Trim out-of-window data
	          -> reorder :: TCPReordering       // Ensure in-order delivery
	          -> procrst :: TCPProcessRst       // Process RST flag
	          -> procsyn :: TCPProcessSyn       // Process SYN flag
	          -> tcp_proc_ack :: TCPClassifier
	          -> procack :: TCPProcessAck
	          -> proctxt :: TCPProcessTxt       // Process segment text
	          -> procfin :: TCPProcessFin       // Process FIN flag
	          -> congcon :: TCPNewRenoAck       // Update cong. control state
	          -> TCPReplacePacket               // Kill old and allocate new pkt
	          -> TCPRateControl                 // Control transmission rate and check if an ACK is needed
	          -> snd_ack;
	          
	          congcon[1] -> snd_rtx;
	            
			  tcp_proc_ack[1] 
			    -> dctcpprocack ::DCTCPProcessAck
				-> proctxt1 :: TCPProcessTxt       // Process segment text
	            -> procfin1 :: TCPProcessFin       // Process FIN flag
	            -> dctcpcongcon :: DCTCPNewRenoAck       // Update cong. control state
	            -> TCPReplacePacket               // Kill old and allocate new pkt
	            -> TCPRateControl                 // Control transmission rate and check if an ACK is needed
	            -> snd_ack;
	            
	          dctcpcongcon[1] -> snd_rtx;
	          
			  tcp_proc_ack[2] 
	            -> bbrprocack :: BBRTCPProcessAck       // Process ACK flag
	            -> procack1 :: TCPProcessAck 
	            -> proctxt2 :: TCPProcessTxt       // Process segment text
	            -> procfin2 :: TCPProcessFin       // Process FIN flag
	            -> bbrcongcon :: BBRTCPAck       // Update cong. control state
	            -> TCPReplacePacket               // Kill old and allocate new pkt
	            -> TCPRateControl                 // Control transmission rate and check if an ACK is needed
	            -> snd_ack;
	            
	         bbrcongcon[1] -> snd_rtx;
	          	
	         optpars[1] -> TCPReplacePacket -> snd_ack;
	         ckseqno[1] -> TCPReplacePacket -> snd_ack;
	             
	         procsyn[1] -> TCPReplacePacket -> snd_rst;
	         procack[1] -> TCPReplacePacket -> snd_ack;
	         procack[2] -> TCPReplacePacket -> snd_rst;
	         procack[3] -> snd_rtr;
	             
             procack1[1] -> TCPReplacePacket -> snd_ack;
             procack1[2] -> TCPReplacePacket -> snd_rst;
             procack1[3] -> snd_rtr;

		     dctcpprocack[1] -> TCPReplacePacket -> snd_ack;
		     dctcpprocack[2] -> TCPReplacePacket -> snd_rst;
		     dctcpprocack[3] -> snd_rtr;
	         reorder[1] -> snd_ack; 
	           
}



