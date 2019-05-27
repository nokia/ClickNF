/*
 * BBRState.{cc,hh} -- BBRState Congestion Control draft-cardwell-iccrg-bbr-congestion-control-00
 * Myriana Rifai
 *
 * Copyright (c) 2018 Nokia Bell Labs
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
 *    in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef CLICK_BBRState_HH
#define CLICK_BBRState_HH
#include <click/packet.hh>
#include "../util.hh"
#include "../tcpstate.hh"
#include "../pktqueue.hh"
#include "ratesample.hh"
#define BW_SCALE 24
#define BW_UNIT (1 << BW_SCALE)
#define CYCLE_LEN	8	/* number of phases in a pacing gain cycle */
#define BBR_SCALE 8	/* scaling factor for fractions in BBR (e.g. gains) */
#define BBR_UNIT (1 << BBR_SCALE)
#define TCP_CA_Open		(1<<0)
#define TCP_CA_Disorder 	(1<<1)
#define TCP_CA_CWR			(1<<2)
#define TCP_CA_Recovery 	(1<<3)
#define TCP_CA_Loss		(1<<4)
CLICK_DECLS

class TCPState;
class RateSample;

const uint32_t 	BtlBwFilterLen = 10, 						// length of the BBRState.BtlBw max filter window for BBRState.BtlBwFilter,
				RTpropFilterLen = 10,  						// length of the RTProp min filter window in seconds
				MinTargetCwnd = 4,
				HighGain = BBR_UNIT * 2885 / 1000 + 1,
				Drain_Gain = BBR_UNIT * 1000 / 2885,
				Cwnd_Gain  = BBR_UNIT * 2, 					// gain for deriving steady-state cwnd tolerates delayed/stretched ACKs:
				ProbeRTTInterval = 10, 						// minimum time interval between ProbeRTT states in seconds
				ProbeRTTDuration = 200, 					// minimum duration for which ProbeRTT state holds inflight to MinTargetCwnd or fewer packets in ms
				FullBwThresh = BBR_UNIT * 5 / 4,
				FullBwCnt = 3,
				CycleRand = 7,								// Randomize the starting gain cycling phase over N phases:
				TsoRate = 1200000,
				LtIntvlMinRtts = 4, 						// The minimum number of rounds in an LT bw sampling interval:
				LtLossThresh = 50, 						// If lost/delivered ratio > 20%, interval is "lossy" and we may be policed:
				LtBwRatio = BBR_UNIT / 8, 				// If 2 intervals have a bw ratio <= 1/8, their bw is "consistent":
				LtBwDiff = 4000 / 8, 						// If 2 intervals have a bw diff <= 4 Kbit/sec their bw is "consistent":
				LtBwMaxRtts = 48, 						// If we estimate we're policed, use lt_bw for this many round trips:
				PacingMarginPercent = 1;					// Pace at ~1% below estimated bw, on average, to reduce queue at bottleneck.

	/* The pacing_gain values for the PROBE_BW gain cycle, to discover/share bw: */
	const uint32_t bbr_pacing_gain[] = {
		BBR_UNIT * 5 / 4,	/* probe for more available bw */
		BBR_UNIT * 3 / 4,	/* drain queue and/or yield bw to other flows */
		BBR_UNIT, BBR_UNIT, BBR_UNIT,	/* cruise at 1.0*bw to utilize pipe, */
		BBR_UNIT, BBR_UNIT, BBR_UNIT	/* without creating excess queue... */
	};

class BBRState { public:
	BBRState(TCPState *s);
	~BBRState();

	void init(TCPState *s); // on connection init
	void update_model_paramters_states(TCPState *s); //on ack
	void handle_restart_from_idle(TCPState *s); //on transmit
	void save_cwnd(TCPState *s);
	bool modulate_cwnd_for_recovery(TCPState *s, uint32_t acked);
	void restore_cwnd(TCPState *s);

	struct minmax btl_bw;
	uint64_t 	pacing_rate, 			// The current pacing rate for a BBRState flow, which controls inter-packet spacing.
				cycle_ustamp,	    	// time of this cycle phase start
				probe_rtt_done_stamp,   // end time for BBRState_PROBE_RTT mode
				rtprop_stamp; 			// The wall clock time at which the current BBRState.RTProp sample was obtained.

	uint8_t		gain_cycle_len, 	//the number of phases in the BBRState ProbeBW gain cycle
				ca_state,
				prev_ca_state:3,
				pacing_shift;
	uint32_t 	prior_cwnd,
				full_bw,				// recent bw, to estimate if pipe is full
				rtprop, 				// estimated two-way round-trip propagation delay of the path, estimated from the windowed minimum recent round-trip delay sample.
				send_quantum, 			// The maximum size of a data aggregate scheduled and transmitted together
				delivered,   			// LT intvl start: tp->delivered
				target_cwnd, 			// prior cwnd upon entering loss recovery
				full_bw_cnt:2,		// number of rounds without large bw gains */
				next_round_delivered, 	// packet.delivered value denoting the end of a packet-timed round trip.
				min_pipe_cwnd, 			// The minimal cwnd value BBRState tries to target using: 4 packets, or 4 * SMSS
				round_count,   			//Count of packet-timed round trips.
				btl_bw_filter,			//The max filter used to estimate BBRState.BtlBw.
				packets_lost,
				initial_cwnd,
				pacing_gain:10, 			// The dynamic gain factor used to scale BBRState.BtlBw to produce BBRState.pacing_rate.
				cycle_idx:3,
				lt_is_sampling,    	// taking long-term ("LT") samples now?
				lt_rtt_cnt:7,	     	// round trips in long-term interval
				lt_use_bw,	     	// use lt_bw as our bw estimate?
				lt_bw,		     		// LT est delivery rate in pkts/uS << 24
				lt_last_delivered,   	// LT intvl start: s->delivered
				lt_last_stamp,	     	// LT intvl start: s->delivered_mstamp
				lt_last_lost,	     	// LT intvl start: s->snd_rtx_count
				cwnd_gain:10;				//The dynamic gain factor used to scale the estimated BDP to produce a congestion window (cwnd).

	uint8_t 	packet_conservation:1, 	 // use packet conservation?
				idle_restart:1 ,	    	 // restarting after idle?
				probe_rtt_round_done:1,  	 // a BBRState_PROBE_RTT round at 4 pkts?
				rtprop_expired:1, 				 // BBRState.RTprop has expired ?
				filled_pipe:1, 					 // fully utilized its available bandwidth?
				round_start:1;

	/* BBRState has the following modes for deciding how fast to send: */
	enum bbr_mode {
		BBRState_STARTUP,	/* ramp up sending rate rapidly to fill pipe */
		BBRState_DRAIN,	/* drain any queue created during startup */
		BBRState_PROBE_BW,	/* discover, share bw: pace around estimated bw */
		BBRState_PROBE_RTT,	/* cut inflight to min to probe min_rtt */
	};

	bbr_mode 	state;
	PktQueue pcq;
  protected:
	void init_pacing_rate(TCPState *s);

	void update_btl_bw(TCPState *s);
	void update_round(TCPState *s);
	void update_rtprop(TCPState *s);
	void update_gains();

	void enter_startup();
	void enter_probe_bw(TCPState *s);
	void enter_probe_rtt();

	void exit_probe_rtt(TCPState *s);

	void check_full_pipe(const RateSample *rs);
	void check_drain(TCPState *s);
	void check_cycle_phase(TCPState *s);

	void set_pacing_rate(TCPState *s, uint32_t bw, uint32_t pacing_gain);
	void set_send_quantum(TCPState *s);
	uint32_t set_target_cwnd(uint32_t bw, uint32_t gain);
	void set_cwnd(TCPState *s);

	void modulate_cwnd_for_probe_rtt(TCPState *s);

	void advance_cycle_phase(TCPState *s);
	bool is_next_cycle_phase(TCPState *s);

	uint32_t packets_in_net_at_edt(TCPState *s, uint32_t inflight_now);

	void reset_lt_bw_sampling_interval(TCPState *s);
	void reset_lt_bw_sampling(TCPState *s);
	void lt_bw_interval_done(TCPState *s, uint32_t bw);
	void lt_bw_sampling(TCPState *s);
	void reset_probe_bw_mode(TCPState *s);

	uint64_t rate_bits_per_sec(TCPState *s, uint64_t rate, int gain);
	uint64_t bw_to_pacing_rate(TCPState *s, uint32_t bw, uint32_t gain);
	uint32_t max_bw();
};
	

CLICK_ENDDECLS
#endif
