elementclass Pi2AQM_TCPPRAGUE { __REST__ $rest |

    PI2Info();
	input[0] -> SetTimestamp
             -> ecn_q :: ECNENQUEUE(900);
    
    pi2:: PI2(W 32, A 20, B 200, K 2, TSHIFT 40, TARGET 20) -> Unqueue() -> [0]output;
    
    ecn_q[0] -> cq :: Queue(1000) -> [0]pi2;
    ecn_q[1] -> lq :: Queue(1000) -> [1]pi2;
    
    
}



