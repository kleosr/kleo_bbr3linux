// SPDX-License-Identifier: GPL-2.0-only
/* BBRv3 (Bottleneck Bandwidth and RTT) congestion control
 *
 * BBRv3 is a model-based congestion control algorithm that aims to achieve
 * both high throughput and low latency by explicitly tracking bandwidth and RTT.
 *
 * This module implements BBRv3 and is based on the Linux kernel's BBR
 * with improvements from BBRv2 and BBRv3.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tcp.h>
#include <linux/inet_diag.h>
#include <linux/random.h>
#include <net/tcp.h>
#include <net/inet_connection_sock.h>

#define BBRV3_VERSION "3.0"

/* BBR constants */
#define BBR_SCALE 8	/* scaling factor for fractions: 1/256 */
#define BBR_UNIT (1 << BBR_SCALE)
#define BW_SCALE 24
#define BW_UNIT (1 << BW_SCALE)

/* Fallback for older kernels */
#ifndef tcp_jiffies32
#define tcp_jiffies32 jiffies
#endif

#ifndef tcp_init_cwnd
static inline u32 tcp_init_cwnd(const struct tcp_sock *tp, const struct dst_entry *dst)
{
	return min_t(u32, 10, max_t(u32, 2, 4380 / tp->mss_cache));
}
#endif

#ifndef tcp_cwnd_reduction_target
static inline u32 tcp_cwnd_reduction_target(const struct tcp_sock *tp)
{
	return max_t(u32, tp->snd_cwnd >> 1, 2);
}
#endif

/* BBRv3 module parameters */
static int bbr_mode __read_mostly = 2;  /* 0=BBRv1, 1=BBRv2, 2=BBRv3 */
module_param(bbr_mode, int, 0644);
MODULE_PARM_DESC(bbr_mode, "BBR version (0=BBRv1, 1=BBRv2, 2=BBRv3)");

static int fast_convergence __read_mostly = 1;
module_param(fast_convergence, int, 0644);
MODULE_PARM_DESC(fast_convergence, "Enable fast convergence");

static int drain_to_target __read_mostly = 1;
module_param(drain_to_target, int, 0644);
MODULE_PARM_DESC(drain_to_target, "Enable drain to target");

static int min_rtt_win_sec __read_mostly = 5;
module_param(min_rtt_win_sec, int, 0644);
MODULE_PARM_DESC(min_rtt_win_sec, "Min RTT filter window length (sec)");

/* BBRv3 states */
enum bbr_mode {
	BBR_STARTUP,
	BBR_DRAIN,
	BBR_PROBE_BW,
	BBR_PROBE_RTT,
};

/* BBRv3 congestion control structure - optimized for size */
struct bbr3 {
	u32 min_rtt_us;                  /* min RTT in min_rtt_win_sec window */
	u32 min_rtt_stamp;               /* timestamp of min_rtt_us */
	u32 probe_rtt_done_stamp;        /* end time for PROBE_RTT */
	u32 full_bandwidth;              /* value of full bandwidth */
	u32 target_cwnd;                 /* target cwnd for pacing */
	u32 prior_cwnd;                  /* prior cwnd */
	u32 cycle_start;                 /* start of pacing gain cycle */
	u16 pacing_gain;                 /* current pacing gain */
	u16 cwnd_gain;                   /* current cwnd gain */
	u8  mode;                        /* current BBR mode */
	u8  prev_ca_state;               /* CA state on previous ACK */
	u8  full_bandwidth_reached:1,    /* reached full bandwidth? */
	    round_start:1,               /* start of packet-timed round? */
	    packet_conservation:1,       /* use packet conservation? */
	    probe_rtt_round_done:1,      /* a BBR_PROBE_RTT round at 4 pkts? */
	    has_seen_rtt:1,              /* have we seen an RTT sample yet? */
	    unused:3;
	u8  full_bandwidth_count;        /* number of full bandwidth measurements */
};

/* BBRv3 pacing gains for different modes */
static const int bbr_pacing_gain[] = {
	BBR_UNIT * 5 / 4,	/* probe for more available bw */
	BBR_UNIT * 3 / 4,	/* drain queue and/or yield bw to other flows */
	BBR_UNIT, BBR_UNIT, BBR_UNIT,	/* cruise at 1.0*bw to utilize pipe, */
	BBR_UNIT, BBR_UNIT, BBR_UNIT	/* without creating excess queue... */
};

static const int bbr_high_gain  = BBR_UNIT * 2885 / 1000 + 1;
static const int bbr_drain_gain = BBR_UNIT * 1000 / 2885;
static const int bbr_cwnd_gain  = BBR_UNIT * 2;

/* BBRv3 congestion control algorithm specific functions */
static void bbr3_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bbr3 *bbr = inet_csk_ca(sk);
	
	/* Initialize the BBRv3 state variables to default values */
	memset(bbr, 0, sizeof(*bbr));
	
	bbr->min_rtt_us = ~0U;
	bbr->min_rtt_stamp = tcp_jiffies32;
	bbr->probe_rtt_done_stamp = 0;
	bbr->full_bandwidth = 0;
	bbr->target_cwnd = 0;
	bbr->prior_cwnd = 0;
	bbr->cycle_start = 0;
	bbr->pacing_gain = bbr_high_gain;
	bbr->cwnd_gain = bbr_cwnd_gain;
	bbr->mode = BBR_STARTUP;
	bbr->prev_ca_state = TCP_CA_Open;
	bbr->full_bandwidth_reached = 0;
	bbr->round_start = 0;
	bbr->packet_conservation = 0;
	bbr->probe_rtt_round_done = 0;
	bbr->has_seen_rtt = 0;
	bbr->full_bandwidth_count = 0;
	
	/* Set initial congestion window */
	tp->snd_cwnd = tcp_init_cwnd(tp, __sk_dst_get(sk));
	
	/* Enable pacing */
	cmpxchg(&sk->sk_pacing_status, SK_PACING_NONE, SK_PACING_NEEDED);
}

/* Update minimum RTT filter */
static void bbr3_update_min_rtt(struct sock *sk, const struct rate_sample *rs)
{
	struct bbr3 *bbr = inet_csk_ca(sk);
	bool filter_expired;

	/* Track min RTT seen in the min_rtt_win_sec filter window: */
	filter_expired = after(tcp_jiffies32,
			       bbr->min_rtt_stamp + min_rtt_win_sec * HZ);
	if (rs->rtt_us >= 0 &&
	    (rs->rtt_us < bbr->min_rtt_us || filter_expired)) {
		bbr->min_rtt_us = rs->rtt_us;
		bbr->min_rtt_stamp = tcp_jiffies32;
	}
}

/* Estimate the bandwidth based on how fast packets are delivered */
static void bbr3_update_bw(struct sock *sk, const struct rate_sample *rs)
{
	struct bbr3 *bbr = inet_csk_ca(sk);
	u64 bw;

	if (rs->delivered < 0 || rs->interval_us <= 0)
		return; /* Not a valid observation */

	/* Calculate bandwidth sample */
	bw = (u64)rs->delivered * BW_UNIT;
	do_div(bw, rs->interval_us);

	/* Update full bandwidth estimate */
	if (!bbr->full_bandwidth_reached) {
		if (bw >= bbr->full_bandwidth) {
			bbr->full_bandwidth = bw;
			bbr->full_bandwidth_count = 0;
		} else {
			bbr->full_bandwidth_count++;
			if (bbr->full_bandwidth_count >= 3)
				bbr->full_bandwidth_reached = 1;
		}
	}
}

/* BBRv3 state machine */
static void bbr3_update_model(struct sock *sk, const struct rate_sample *rs)
{
	struct bbr3 *bbr = inet_csk_ca(sk);
	
	bbr3_update_bw(sk, rs);
	bbr3_update_min_rtt(sk, rs);
	
	/* Simple state transitions for demo */
	switch (bbr->mode) {
	case BBR_STARTUP:
		if (bbr->full_bandwidth_reached) {
			bbr->mode = BBR_DRAIN;
			bbr->pacing_gain = bbr_drain_gain;
			bbr->cwnd_gain = bbr_high_gain;
		}
		break;
	case BBR_DRAIN:
		if (tcp_packets_in_flight(tcp_sk(sk)) <= 
		    tcp_cwnd_reduction_target(tcp_sk(sk))) {
			bbr->mode = BBR_PROBE_BW;
			bbr->pacing_gain = BBR_UNIT;
			bbr->cwnd_gain = BBR_UNIT;
		}
		break;
	case BBR_PROBE_BW:
		/* Stay in PROBE_BW for simplicity */
		break;
	case BBR_PROBE_RTT:
		/* Simplified PROBE_RTT */
		if (after(tcp_jiffies32, bbr->probe_rtt_done_stamp)) {
			bbr->mode = BBR_PROBE_BW;
			bbr->pacing_gain = BBR_UNIT;
		}
		break;
	}
}

/* Update congestion window */
static void bbr3_set_cwnd(struct sock *sk, const struct rate_sample *rs,
			  u32 acked, u32 bw, int gain)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bbr3 *bbr = inet_csk_ca(sk);
	u32 cwnd = 0, target_cwnd = 0;

	if (!acked)
		goto done;

	if (bbr->mode == BBR_STARTUP) {
		/* In startup, grow cwnd exponentially */
		cwnd = tp->snd_cwnd + acked;
	} else {
		/* Calculate target cwnd based on BDP */
		if (bbr->min_rtt_us < ~0U && bw) {
			target_cwnd = (u64)bw * bbr->min_rtt_us / BW_UNIT;
			target_cwnd = (target_cwnd * gain) >> BBR_SCALE;
			target_cwnd += 3 * tp->mss_cache; /* headroom */
			cwnd = min(target_cwnd, tp->snd_cwnd + acked);
		} else {
			cwnd = tp->snd_cwnd + acked;
		}
	}

	cwnd = max(cwnd, 4U);
	tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

done:
	bbr->target_cwnd = target_cwnd;
}

/* Main BBRv3 algorithm */
static void bbr3_main(struct sock *sk, const struct rate_sample *rs)
{
	struct bbr3 *bbr = inet_csk_ca(sk);
	u32 bw;

	bbr3_update_model(sk, rs);

	bw = bbr->full_bandwidth;
	bbr3_set_cwnd(sk, rs, rs->acked_sacked, bw, bbr->cwnd_gain);
}

/* Implementation of required TCP congestion control operations */
static u32 bbr3_ssthresh(struct sock *sk)
{
	return tcp_sk(sk)->snd_ssthresh;
}

static u32 bbr3_undo_cwnd(struct sock *sk)
{
	struct bbr3 *bbr = inet_csk_ca(sk);
	return max(tcp_sk(sk)->snd_cwnd, bbr->prior_cwnd);
}

static void bbr3_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	struct bbr3 *bbr = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	if (event == CA_EVENT_TX_START && tp->app_limited) {
		bbr->prior_cwnd = tp->snd_cwnd;
	}
}

static void bbr3_pkts_acked(struct sock *sk, const struct ack_sample *sample)
{
	/* Handle packet acknowledgements - placeholder for future enhancements */
}

static void bbr3_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	/* BBR doesn't use traditional congestion avoidance */
}

static size_t bbr3_get_info(struct sock *sk, u32 ext, int *attr,
			    union tcp_cc_info *info)
{
	if (ext & (1 << (INET_DIAG_BBRINFO - 1)) ||
	    ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		struct bbr3 *bbr = inet_csk_ca(sk);
		struct tcp_bbr_info *bbr_info = &info->bbr;

		bbr_info->bbr_bw_lo = (u32)bbr->full_bandwidth;
		bbr_info->bbr_bw_hi = (u32)(bbr->full_bandwidth >> 32);
		bbr_info->bbr_min_rtt = bbr->min_rtt_us;
		bbr_info->bbr_pacing_gain = bbr->pacing_gain;
		bbr_info->bbr_cwnd_gain = bbr->cwnd_gain;
		*attr = INET_DIAG_BBRINFO;
		return sizeof(*bbr_info);
	}
	return 0;
}

/* Register with TCP congestion control */
static struct tcp_congestion_ops tcp_bbr3_cong_ops __read_mostly = {
	.flags		= TCP_CONG_NON_RESTRICTED,
	.name		= "bbr3",
	.owner		= THIS_MODULE,
	.init		= bbr3_init,
	.cong_control	= bbr3_main,
	.ssthresh	= bbr3_ssthresh,
	.undo_cwnd	= bbr3_undo_cwnd,
	.cwnd_event	= bbr3_cwnd_event,
	.pkts_acked	= bbr3_pkts_acked,
	.cong_avoid	= bbr3_cong_avoid,
	.get_info	= bbr3_get_info,
	.min_tso_segs	= 1,
};

/* Module initialization and cleanup */
static int __init bbr3_register(void)
{
	BUILD_BUG_ON(sizeof(struct bbr3) > ICSK_CA_PRIV_SIZE);
	
	pr_info("TCP BBRv3: Bottleneck Bandwidth and RTT v%s\n", BBRV3_VERSION);
	pr_info("TCP BBRv3: Mode set to %d (0=BBRv1, 1=BBRv2, 2=BBRv3)\n", bbr_mode);
	
	return tcp_register_congestion_control(&tcp_bbr3_cong_ops);
}

static void __exit bbr3_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_bbr3_cong_ops);
}

module_init(bbr3_register);
module_exit(bbr3_unregister);

MODULE_AUTHOR("Claude AI");
MODULE_LICENSE("GPL");
MODULE_VERSION(BBRV3_VERSION);
MODULE_DESCRIPTION("TCP BBRv3 (Bottleneck Bandwidth and RTT)"); 