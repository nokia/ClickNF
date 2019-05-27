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

#include <click/config.h>
#include <click/machine.hh>
#include "bbrstate.hh"
#include <algorithm>
#include <random>
#define SRTT_DEFAULT 1000
#define KILOBYTE 1000
#define MBPS 1000000
#define SEC_TO_USEC 1000000
CLICK_DECLS

BBRState::BBRState(TCPState *s) :
		pacing_rate(0), cycle_ustamp(0), probe_rtt_done_stamp(0), rtprop_stamp(
				Timestamp::now_steady().usecval()), gain_cycle_len(8), prev_ca_state(
		TCP_CA_Open), pacing_shift(0), prior_cwnd(0), full_bw(0), rtprop(
				s->snd_srtt ? s->snd_srtt : INFINITY), send_quantum(0), delivered(
				0), target_cwnd(0), full_bw_cnt(0), next_round_delivered(0), min_pipe_cwnd(
				0), round_count(0), btl_bw_filter(0), packets_lost(0), initial_cwnd(
				0), pacing_gain(HighGain), cycle_idx(3), lt_is_sampling(1), lt_rtt_cnt(
				7), lt_use_bw(1), lt_bw(0), lt_last_delivered(0), lt_last_stamp(
				0), lt_last_lost(0), cwnd_gain(HighGain), packet_conservation(
				0), idle_restart(0), probe_rtt_round_done(0), rtprop_expired(0), filled_pipe(
				0), round_start(0), state(BBRState_STARTUP) {
	init(s);
}

BBRState::~BBRState() {
}

void BBRState::init(TCPState *s) {
	minmax_reset(&btl_bw, round_count, 0); /* init max bw to 0 */
	init_pacing_rate(s);
	reset_lt_bw_sampling(s);
	enter_startup();
}

/* Return rate in bytes per second, optionally with a gain.
 * The order here is chosen carefully to avoid overflow of u64. This should
 * work for input rates of up to 2.9Tbit/sec and gain of 2.89x.
 */
uint64_t BBRState::rate_bits_per_sec(TCPState *s, uint64_t rate, int gain) {

	rate *= s->snd_mss;
	rate *= gain;
	rate >>= BBR_SCALE;
	rate *= Timestamp::usec_per_sec / 100 * (100 - PacingMarginPercent);
	return rate >> BW_SCALE;
}

/* Convert a BBR bw and gain factor to a pacing rate in bytes per second. */
uint64_t BBRState::bw_to_pacing_rate(TCPState *s, uint32_t bw, uint32_t gain) {
	uint64_t rate = bw;
	rate = rate_bits_per_sec(s, rate, gain);
	//click_chatter("rate = %u", rate);
	return rate;
}

void BBRState::init_pacing_rate(TCPState *s) {
	uint32_t srtt_us;
	if (s->snd_srtt)
		srtt_us = std::max(s->snd_srtt >> 3, 1U); //nominal_bandwidth = InitialCwnd / (SRTT ? SRTT : 1ms)
	else
		srtt_us = SRTT_DEFAULT;
	uint64_t nominal_bw = (uint64_t) (s->snd_cwnd * BW_UNIT / srtt_us);
	pacing_rate = bw_to_pacing_rate(s, nominal_bw, HighGain);
}

void BBRState::enter_startup() {
	state = BBRState_STARTUP;
}

// ON ACK AND PACKET ARRIVAL
void BBRState::update_model_paramters_states(TCPState *s) {
	update_round(s);
	update_btl_bw(s);
	check_cycle_phase(s);
	check_full_pipe(s->rs);
	check_drain(s);
	update_rtprop(s);
	update_gains();
	set_pacing_rate(s, max_bw(), pacing_gain);
	set_send_quantum(s);
	set_cwnd(s);
}

void BBRState::update_round(TCPState *s) {
	round_start = 0;
	if (s->rs->delivered <= 0 || s->rs->interval_us <= 0)
		return; /* Not a valid observation */

	/* See if we've reached the next RTT */
	if (s->rs->prior_delivered >= next_round_delivered) {
		next_round_delivered = s->delivered;
		round_count++;
		round_start = 1;
		packet_conservation = 0;
	}

}

/* Return the windowed max recent bandwidth sample, in pkts/uS << BW_SCALE. */
uint32_t BBRState::max_bw() {
	return minmax_get(&btl_bw);
}

/* Start a new long-term sampling interval. */
void BBRState::reset_lt_bw_sampling_interval(TCPState *s) {

	lt_last_stamp = (uint64_t) (s->delivered_ustamp / Timestamp::usec_per_msec);
	lt_last_delivered = s->delivered;
	lt_last_lost = s->snd_rtx_count;
	lt_rtt_cnt = 0;
}

/* Completely reset long-term bandwidth sampling. */
void BBRState::reset_lt_bw_sampling(TCPState *s) {

	lt_bw = 0;
	lt_use_bw = 0;
	lt_is_sampling = 0;
	reset_lt_bw_sampling_interval(s);
}

/* Long-term bw sampling interval is done. Estimate whether we're policed. */
void BBRState::lt_bw_interval_done(TCPState *s, uint32_t bw) {
	uint32_t diff;

	if (lt_bw) { /* do we have bw from a previous interval? */
		/* Is new bw close to the lt_bw from the previous interval? */
		diff = fabs(bw - lt_bw);
		if ((diff * BBR_UNIT <= LtBwRatio * lt_bw)
				|| (rate_bits_per_sec(s, diff, BBR_UNIT) <= LtBwDiff)) {
			/* All criteria are met; estimate we're policed. */
			lt_bw = (bw + lt_bw) >> 1; /* avg 2 intvls */
			lt_use_bw = 1;
			pacing_gain = BBR_UNIT; /* try to avoid drops */
			lt_rtt_cnt = 0;
			return;
		}
	}
	lt_bw = bw;
	reset_lt_bw_sampling_interval(s);
}

void BBRState::reset_probe_bw_mode(TCPState *s) {
	state = BBRState_PROBE_BW;
	cycle_idx = CYCLE_LEN - 1 - (uint32_t) (rand() % CycleRand);
	advance_cycle_phase(s); /* flip to next phase of gain cycle */
}

/* Token-bucket traffic policers are common (see "An Internet-Wide Analysis of
 * Traffic Policing", SIGCOMM 2016). BBR detects token-bucket policers and
 * explicitly models their policed rate, to reduce unnecessary losses. We
 * estimate that we're policed if we see 2 consecutive sampling intervals with
 * consistent throughput and high packet loss. If we think we're being policed,
 * set lt_bw to the "long-term" average delivery rate from those 2 intervals.
 */
void BBRState::lt_bw_sampling(TCPState *s) {
	uint32_t lost, delivered;
	uint64_t bw;
	uint64_t t;

	if (lt_use_bw) { /* already using long-term rate, lt_bw? */
		if (state == BBRState_PROBE_BW && round_start
				&& ++lt_rtt_cnt >= LtBwMaxRtts) {
			reset_lt_bw_sampling(s); /* stop using lt_bw */
			reset_probe_bw_mode(s); /* restart gain cycling */
		}
		return;
	}

	/* Wait for the first loss before sampling, to let the policer exhaust
	 * its tokens and estimate the steady-state rate allowed by the policer.
	 * Starting samples earlier includes bursts that over-estimate the bw.
	 */
	if (!lt_is_sampling) {
		if (!s->snd_rtx_count)
			return;
		reset_lt_bw_sampling_interval(s);
		lt_is_sampling = 1;
	}

	/* To avoid underestimates, reset sampling if we run out of data. */
	if (s->rs->is_app_limited) {
		reset_lt_bw_sampling(s);
		return;
	}

	if (round_start)
		lt_rtt_cnt++; /* count round trips in this interval */
	if (lt_rtt_cnt < LtIntvlMinRtts)
		return; /* sampling interval needs to be longer */
	if (lt_rtt_cnt > 4 * LtIntvlMinRtts) {
		reset_lt_bw_sampling(s); /* interval is too long */
		return;
	}

	/* End sampling interval when a packet is lost, so we estimate the
	 * policer tokens were exhausted. Stopping the sampling before the
	 * tokens are exhausted under-estimates the policed rate.
	 */
	if (!s->snd_rtx_count)
		return;

	/* Calculate packets lost and delivered in sampling interval. */
	lost = s->snd_rtx_count - lt_last_lost;
	delivered = s->delivered - lt_last_delivered;
	/* Is loss rate (lost/delivered) >= lt_loss_thresh? If not, wait. */
	if (!delivered || (lost << BBR_SCALE) < LtLossThresh * delivered)
		return;

	/* Find average delivery rate in this sampling interval. */
	t = (uint64_t) (s->delivered_ustamp / Timestamp::usec_per_msec)
			- lt_last_stamp;
	if ((signed int) t < 1)
		return; /* interval is less than one ms, so wait */
	/* Check if can multiply without overflow */
	if (t >= ~0U / Timestamp::usec_per_msec) {
		reset_lt_bw_sampling(s); /* interval too long; reset */
		return;
	}
	t *= Timestamp::usec_per_msec;
	bw = (uint64_t) delivered * BW_UNIT;
	bw = bw / t;
	lt_bw_interval_done(s, bw);
}

/* Estimate the bandwidth based on how fast packets are delivered */
void BBRState::update_btl_bw(TCPState *s) {
	uint64_t bw;
	lt_bw_sampling(s);

	/* Divide delivered by the interval to find a (lower bound) bottleneck
	 * bandwidth sample. Delivered is in packets and interval_us in uS and
	 * ratio will be <<1 for most connections. So delivered is first scaled.
	 */
	bw = (uint64_t) s->rs->delivered * BW_UNIT;
	bw = bw / s->rs->interval_us;

	/* If this sample is application-limited, it is likely to have a very
	 * low delivered count that represents application behavior rather than
	 * the available network rate. Such a sample could drag down estimated
	 * bw, causing needless slow-down. Thus, to continue to send at the
	 * last measured network rate, we filter out app-limited samples unless
	 * they describe the path bw at least as well as our bw model.
	 *
	 * So the goal during app-limited phase is to proceed with the best
	 * network rate no matter how long. We automatically leave this
	 * phase when app writes faster than the network can deliver :)
	 */
	if (!s->rs->is_app_limited || bw >= max_bw()) {
		/* Incorporate new sample into our max bw filter. */
		minmax_running_max(&btl_bw, BtlBwFilterLen, round_count, bw);
	}
	//click_chatter("new bw = %u",bw);
}

void BBRState::check_cycle_phase(TCPState *s) {
	if (state == BBRState_PROBE_BW && is_next_cycle_phase(s))
		advance_cycle_phase(s);
}

/* End cycle phase if it's time and/or we hit the phase's in-flight target. */
bool BBRState::is_next_cycle_phase(TCPState *s) {

	bool is_full_length = (s->delivered_ustamp - cycle_ustamp) > rtprop;
	uint32_t inflight;

	/* The pacing_gain of 1.0 paces at the estimated bw to try to fully
	 * use the pipe without increasing the queue.
	 */
	if (pacing_gain == BBR_UNIT)
		return is_full_length; /* just use wall clock time */

	inflight = packets_in_net_at_edt(s, s->rs->prior_in_flight);

	/* A pacing_gain > 1.0 probes for bw by trying to raise inflight to at
	 * least pacing_gain*BDP; this may take more than min_rtt if min_rtt is
	 * small (e.g. on a LAN). We do not persist if packets are lost, since
	 * a path with small buffers may not hold that much.
	 */
	if (pacing_gain > BBR_UNIT)
		return is_full_length && (s->snd_rtx_count || /* perhaps pacing_gain*BDP won't fit */
		inflight >= set_target_cwnd(max_bw(), pacing_gain));

	/* A pacing_gain < 1.0 tries to drain extra queue we added if bw
	 * probing didn't find more bw. If inflight falls to match BDP then we
	 * estimate queue is drained; persisting would underutilize the pipe.
	 */
	return is_full_length || inflight <= set_target_cwnd(max_bw(), BBR_UNIT);
}

void BBRState::check_full_pipe(const RateSample *rs) {

	uint32_t bw_thresh;

	if (filled_pipe || !round_start || rs->is_app_limited)
		return;

	bw_thresh = (uint64_t) full_bw * FullBwThresh >> BBR_SCALE;
	if (max_bw() >= bw_thresh) {
		full_bw = max_bw();
		full_bw_cnt = 0;
		return;
	}
	++full_bw_cnt;
	filled_pipe = full_bw_cnt >= FullBwCnt;
}

void BBRState::check_drain(TCPState *s) {
	if (state == BBRState_STARTUP && filled_pipe) {
		state = BBRState_DRAIN; /* drain queue we created */
		s->snd_cwnd = set_target_cwnd(max_bw(), BBR_UNIT);
	} /* fall through to check if in-flight is already small: */
	else if (state == BBRState_DRAIN
			&& packets_in_net_at_edt(s, s->tcp_packets_in_flight())
					<= set_target_cwnd(max_bw(), BBR_UNIT))
		enter_probe_bw(s); /* we estimate queue is drained */

}

void BBRState::enter_probe_bw(TCPState *s) {
	state = BBRState_PROBE_BW;
	cycle_idx = CYCLE_LEN - 1 - (uint32_t) (rand() % CycleRand);
	advance_cycle_phase(s); /* flip to next phase of gain cycle */
}

void BBRState::advance_cycle_phase(TCPState *s) {
	cycle_ustamp = s->delivered_ustamp;
	cycle_idx = (cycle_idx + 1) & (CYCLE_LEN - 1);
}

uint32_t BBRState::set_target_cwnd(uint32_t bw, uint32_t gain) {
	uint32_t cwnd;
	/* If we've never had a valid RTT sample, cap cwnd at the initial
	 * default. This should only happen when the connection is not using TCP
	 * timestamps and has retransmitted all of the SYN/SYNACK/data packets
	 * ACKed so far. In this case, an RTO can cut cwnd to 1, in which
	 * case we need to slow-start up toward something safe: TCP_INIT_CWND.
	 */
	if (unlikely(rtprop == ~0U))
		return initial_cwnd;
	/* Apply a gain to the given value, then remove the BW_SCALE shift. */
	cwnd = (((bw * rtprop * gain ) >> BBR_SCALE) + BW_UNIT - 1) / BW_UNIT;
	/* Allow enough full-sized skbs in flight to utilize end systems. */
	cwnd += 3 * send_quantum;

	/* Reduce delayed ACKs by rounding up cwnd to the next even number. */
	cwnd = (cwnd + 1) & ~1U;

	/* Ensure gain cycling gets inflight above BDP even for small BDPs. */
	if (state == BBRState_PROBE_BW && gain > BBR_UNIT)
		cwnd += 2;
	return cwnd;
}

void BBRState::handle_restart_from_idle(TCPState *s) {
	if (packets_in_net_at_edt(s, s->rs->prior_delivered)
			&& s->rs->is_app_limited) {
		idle_restart = 1;
		/* Avoid pointless buffer overflows: pace at est. bw if we don't
		 * need more speed (we're restarting from idle and app-limited).
		 */
		if (state == BBRState_PROBE_BW)
			set_pacing_rate(s, max_bw(), BBR_UNIT);
	}
}

void BBRState::update_rtprop(TCPState *s) {

	// Track min RTT seen in the min_rtt_win_sec filter window:
	rtprop_expired =
			(uint64_t) Timestamp::now_steady().usecval() > rtprop_stamp + RTpropFilterLen * SEC_TO_USEC;

	// Calculating rtt in the estimator and the value is sent here only upon condition of !is_delayed_ack
	if (s->last_rtt > 0 && (s->last_rtt <= rtprop || (rtprop_expired))) {
		rtprop = s->last_rtt;
		rtprop_stamp = Timestamp::now_steady().usecval();
	}

	if (state != BBRState_PROBE_RTT && rtprop_expired && !idle_restart
			&& ProbeRTTDuration > 0) {
		enter_probe_rtt();
		save_cwnd(s);
		probe_rtt_done_stamp = 0;
	} else if (state == BBRState_PROBE_RTT) {
		s->app_limited = (s->delivered + s->tcp_packets_in_flight()) ? : 1;
		if (!probe_rtt_done_stamp
				&& s->tcp_packets_in_flight() <= MinTargetCwnd) {
			probe_rtt_done_stamp = (uint64_t)Timestamp::now_steady().usecval()
					+ (ProbeRTTDuration * 1000);
			//click_chatter("setting round probe rtt at t= %u", Timestamp::now_steady().usecval());
			probe_rtt_round_done = 0;
			next_round_delivered = s->delivered;
		} else if (probe_rtt_done_stamp) {
			if (round_start){
				probe_rtt_round_done = 1;
			}
			if (probe_rtt_round_done
					&& (probe_rtt_done_stamp
							&& (uint64_t) Timestamp::now_steady().usecval()
									> probe_rtt_done_stamp)) {
				//click_chatter("exiting at %u", probe_rtt_done_stamp);
				rtprop_stamp = Timestamp::now_steady().usecval();
				restore_cwnd(s);
				exit_probe_rtt(s);
			}
		}
	}

	if (s->rs->delivered > 0)
		idle_restart = 0;

}

void BBRState::set_pacing_rate(TCPState *s, uint32_t bw, uint32_t pacing_gain) {
	uint64_t rate = bw_to_pacing_rate(s, bw, pacing_gain);
	if (filled_pipe || rate > pacing_rate)
		pacing_rate = rate;
}

void BBRState::set_send_quantum(TCPState *s) {
	if (pacing_rate < TsoRate)
		send_quantum = 1 * s->snd_mss;
	else if (pacing_rate < 24 * MBPS)
		send_quantum = 2 * s->snd_mss;
	else
		send_quantum = std::min((uint64_t)(pacing_rate >> BBR_SCALE / SRTT_DEFAULT),
				(uint64_t) (64 * KILOBYTE));
}

/* Save "last known good" cwnd so we can restore it after losses or PROBE_RTT */
void BBRState::save_cwnd(TCPState *s) {

	if (prev_ca_state < TCP_CA_Recovery && state != BBRState_PROBE_RTT){
		prior_cwnd = s->snd_cwnd;
	}
	else
		prior_cwnd = std::max(prior_cwnd, s->snd_cwnd);

}

void BBRState::restore_cwnd(TCPState *s) {
	s->snd_cwnd = std::max(s->snd_cwnd, prior_cwnd);
}

bool BBRState::modulate_cwnd_for_recovery(TCPState *s, uint32_t acked) {
	uint32_t cwnd = s->snd_cwnd;
	if (s->snd_rtx_count > 0)
		cwnd = std::max(cwnd - (s->snd_rtx_count * s->snd_mss), (MinTargetCwnd * s->snd_mss));
	if (ca_state == TCP_CA_Recovery && prev_ca_state != TCP_CA_Recovery) {
		packet_conservation = 1;
		next_round_delivered = s->delivered;
		cwnd = (s->tcp_packets_in_flight()* s->snd_mss) + acked;
	} else if (prev_ca_state >= TCP_CA_Recovery && ca_state < TCP_CA_Recovery) {
		restore_cwnd(s);
		packet_conservation = 0;
		return false;
	}
	prev_ca_state = ca_state;
	if (packet_conservation) {
		s->snd_cwnd = std::max(cwnd, (s->tcp_packets_in_flight()* s->snd_mss) + acked);

		return true;
	}
	s->snd_cwnd = cwnd;
	return false;
}

void BBRState::set_cwnd(TCPState *s) {
	uint32_t cwnd = s->snd_cwnd;
	if (s->rs->acked_sacked) {
		if (!modulate_cwnd_for_recovery(s, s->rs->acked_sacked)) {
			target_cwnd = set_target_cwnd(max_bw(), cwnd_gain);
			if (filled_pipe) {
				cwnd = std::min(cwnd + s->rs->acked_sacked, target_cwnd);
			} else if (cwnd < target_cwnd || s->delivered < initial_cwnd) {
				cwnd = cwnd + s->rs->acked_sacked;
			}

		}
	}

	if (state == BBRState_PROBE_RTT) {// when in PROBE_RTT state drain queue
		if (s->snd_cwnd !=  std::min(cwnd, (MinTargetCwnd * s->snd_mss)))
		s->snd_cwnd = std::min(s->snd_cwnd, (MinTargetCwnd * s->snd_mss));
	} else {
			s->snd_cwnd = std::max(cwnd, (MinTargetCwnd * s->snd_mss));
	}

}

void BBRState::update_gains() {
	switch (state) {
	case BBRState_STARTUP:
		pacing_gain = HighGain;
		cwnd_gain = HighGain;
		break;
	case BBRState_DRAIN:
		pacing_gain = Drain_Gain; /* slow, to drain */
		cwnd_gain = HighGain; /* keep cwnd */
		break;
	case BBRState_PROBE_BW:
		pacing_gain = (lt_use_bw ?
		BBR_UNIT :
									bbr_pacing_gain[cycle_idx]);
		cwnd_gain = Cwnd_Gain;
		break;
	case BBRState_PROBE_RTT:
		pacing_gain = BBR_UNIT;
		cwnd_gain = BBR_UNIT;
		break;
	default:
		//WARN_ONCE(1, "BBRState bad state: %u\n", state);
		break;
	}
}

void BBRState::enter_probe_rtt() {
	//click_chatter("enter probe rtt at t=%u",Timestamp::now_steady().usecval());
	state = BBRState_PROBE_RTT;
}

void BBRState::exit_probe_rtt(TCPState *s) {
	if (filled_pipe)
		enter_probe_bw(s);
	else
		enter_startup();

}

/* With pacing at lower layers, there's often less data "in the network" than
 * "in flight". With TSQ and departure time pacing at lower layers (e.g. fq),
 * we often have several skbs queued in the pacing layer with a pre-scheduled
 * earliest departure time (EDT). BBRState adapts its pacing rate based on the
 * inflight level that it estimates has already been "baked in" by previous
 * departure time decisions. We calculate a rough estimate of the number of our
 * packets that might be in the network at the earliest departure time for the
 * next skb scheduled:
 *   in_network_at_edt = inflight_at_edt - (EDT - now) * bw
 * If we're increasing inflight, then we want to know if the transmit of the
 * EDT skb will push inflight above the target, so inflight_at_edt includes
 * bbr_tso_segs_goal() from the skb departing at EDT. If decreasing inflight,
 * then estimate if inflight will sink too low just before the EDT transmit.
 */
uint32_t BBRState::packets_in_net_at_edt(TCPState *s, uint32_t inflight_now) {

	uint64_t now_us, edt_us, interval_us;
	uint32_t interval_delivered, inflight_at_edt;

	now_us = (uint64_t) Timestamp::now_steady().usecval();
	edt_us = std::max((uint64_t) s->next_send_time, now_us);
	interval_us = edt_us - now_us;
	interval_delivered = (uint64_t) max_bw() * interval_us >> BW_SCALE;
	inflight_at_edt = inflight_now;
	if (pacing_gain > BBR_UNIT) /* increasing inflight */
		inflight_at_edt += send_quantum; /* include EDT skb */
	if (interval_delivered >= inflight_at_edt)
		return 0;
	return inflight_at_edt - interval_delivered;
}

CLICK_ENDDECLS
#undef click_assert
ELEMENT_PROVIDES(BBRState)
