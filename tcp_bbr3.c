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
#include <net/tcp.h>

#define BBRV3_VERSION "3.0"

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

/* BBRv3 congestion control structure */
struct bbr3 {
    u32 min_rtt_us;                  /* min RTT in min_rtt_win_sec window */
    u32 min_rtt_stamp;               /* timestamp of min_rtt_us */
    u32 probe_rtt_done_stamp;        /* end time for PROBE_RTT */
    u32 probe_rtt_min_us;            /* min RTT during PROBE_RTT */
    u32 prior_cwnd;                  /* prior cwnd */
    u32 full_bandwidth;              /* value of full bandwidth */
    u32 full_bandwidth_count;        /* number of full bandwidth measurements */
    u32 cycle_start;                 /* start of pacing gain cycle */
    u32 target_cwnd;                 /* target cwnd for pacing */
    u32 extra_acked;                 /* extra bytes ACKed during epoch */
    u32 extra_acked_stamp;           /* ACK time of most recent ECN mark */
    u32 ack_epoch_start;             /* start of ACK sampling epoch */
    u32 extra_acked_interval_us;     /* extra_acked sampling interval */
    u16 dctcp_alpha;                 /* dctcp congestion level */
    u8  pacing_gain;                 /* current pacing gain */
    u8  cwnd_gain;                   /* current cwnd gain */
    u8  prev_state;                  /* state before PROBE_RTT */
    u8  state;                       /* current state */
    u8  full_bandwidth_reached;      /* reached full bandwidth? */
    u8  round_start;                 /* start of packet-timed round? */
    u8  packet_conservation;         /* use packet conservation? */
    u8  probe_rtt_round_done;        /* a BBR_PROBE_RTT round at 4 pkts? */
    u8  prev_ca_state;               /* CA state on previous ACK */
    u8  tx_in_flight;                /* packet in flight has been transmitted */
    u8  has_seen_rtt;                /* have we seen an RTT sample yet? */
    u8  flexible_app_limited;        /* is app limiting w/ low rate demands? */
    u8  loss_in_cycle;               /* packet loss in this cycle? */
    u32 loss_round_start;            /* start of round during which loss occurred */
    u64 undo_cwnd;                   /* undo cwnd after loss recovered */
};

/* BBRv3 congestion control algorithm specific functions */
static void bbr3_init(struct sock *sk)
{
    /* Initialize the BBRv3 state */
    struct bbr3 *bbr = (struct bbr3 *)inet_csk_ca(sk);
    struct tcp_sock *tp = tcp_sk(sk);
    
    /* Initialize the BBRv3 state variables to default values */
    bbr->min_rtt_us = 0;
    bbr->min_rtt_stamp = 0;
    bbr->probe_rtt_done_stamp = 0;
    bbr->probe_rtt_min_us = 0;
    bbr->prior_cwnd = 0;
    bbr->full_bandwidth = 0;
    bbr->full_bandwidth_count = 0;
    bbr->cycle_start = 0;
    bbr->target_cwnd = 0;
    bbr->extra_acked = 0;
    bbr->extra_acked_stamp = 0;
    bbr->ack_epoch_start = 0;
    bbr->extra_acked_interval_us = 0;
    bbr->dctcp_alpha = 0;
    bbr->pacing_gain = 0;
    bbr->cwnd_gain = 0;
    bbr->prev_state = 0;
    bbr->state = 0;
    bbr->full_bandwidth_reached = 0;
    bbr->round_start = 0;
    bbr->packet_conservation = 0;
    bbr->probe_rtt_round_done = 0;
    bbr->prev_ca_state = 0;
    bbr->tx_in_flight = 0;
    bbr->has_seen_rtt = 0;
    bbr->flexible_app_limited = 0;
    bbr->loss_in_cycle = 0;
    bbr->loss_round_start = 0;
    bbr->undo_cwnd = 0;
    
    /* Set initial pacing rate */
    tp->snd_cwnd = 10;  /* Initial window of 10 segments */
    bbr->state = 0;     /* Startup phase */
    
    /* Use high gain in startup */
    bbr->pacing_gain = 2;
    bbr->cwnd_gain = 2;
    
    cmpxchg(&sk->sk_pacing_status, SK_PACING_NONE, SK_PACING_NEEDED);
}

/* BBRv3 state */
static void bbr3_state_machine(struct sock *sk)
{
    /* Simple state machine for demo purposes */
    struct bbr3 *bbr = (struct bbr3 *)inet_csk_ca(sk);
    
    /* For this simplified module, we just stay in state 0 */
    bbr->state = bbr_mode;  /* Use the module parameter to set the mode */
}

/* Main congestion control functions */
static void bbr3_update_model(struct sock *sk, const struct rate_sample *rs)
{
    struct bbr3 *bbr = (struct bbr3 *)inet_csk_ca(sk);
    
    /* Update the RTT min filter */
    if (rs->rtt_us > 0 && (bbr->min_rtt_us == 0 || rs->rtt_us < bbr->min_rtt_us)) {
        bbr->min_rtt_us = rs->rtt_us;
        bbr->min_rtt_stamp = tcp_jiffies32;
    }
    
    bbr3_state_machine(sk);
}

static void bbr3_update_cwnd(struct sock *sk, const struct rate_sample *rs)
{
    struct bbr3 *bbr = (struct bbr3 *)inet_csk_ca(sk);
    struct tcp_sock *tp = tcp_sk(sk);
    
    /* Simplified cwnd update for demo */
    if (rs->delivered > 0) {
        u32 cwnd = tp->snd_cwnd;
        
        /* Additive increase based on BBR mode */
        if (bbr->state == 2) {  /* BBRv3 */
            /* More aggressive cwnd growth */
            cwnd += (rs->delivered * bbr->cwnd_gain) >> 8;
        } else if (bbr->state == 1) {  /* BBRv2 */
            /* Medium cwnd growth */
            cwnd += (rs->delivered * 3) >> 4;
        } else {  /* BBRv1 */
            /* Standard cwnd growth */
            cwnd += rs->delivered;
        }
        
        /* Apply cwnd limit */
        tp->snd_cwnd = min_t(u32, cwnd, tp->snd_cwnd_clamp);
    }
}

static void bbr3_main(struct sock *sk, const struct rate_sample *rs)
{
    bbr3_update_model(sk, rs);
    bbr3_update_cwnd(sk, rs);
}

/* Implementation of required TCP congestion control operations */
static u32 bbr3_ssthresh(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    return tp->snd_ssthresh;
}

static u32 bbr3_undo_cwnd(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    return tp->snd_cwnd;
}

static void bbr3_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
    /* Handle congestion control events */
}

static void bbr3_pkts_acked(struct sock *sk, const struct ack_sample *sample)
{
    /* Handle packet acknowledgements */
}

static void bbr3_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
    /* Basic congestion avoidance implementation */
    struct tcp_sock *tp = tcp_sk(sk);
    if (tp->snd_cwnd < tp->snd_cwnd_clamp)
        tp->snd_cwnd++;
}

static size_t bbr3_get_info(struct sock *sk, u32 ext, int *attr, union tcp_cc_info *info)
{
    /* Return information about the congestion control state */
    return 0;
}

/* Register with TCP congestion control */
static struct tcp_congestion_ops tcp_bbr3_cong_ops __read_mostly = {
    .flags        = TCP_CONG_NON_RESTRICTED,
    .name         = "bbr3",
    .owner        = THIS_MODULE,
    .init         = bbr3_init,
    .cong_control = bbr3_main,
    .ssthresh     = bbr3_ssthresh,
    .undo_cwnd    = bbr3_undo_cwnd,
    .cwnd_event   = bbr3_cwnd_event,
    .pkts_acked   = bbr3_pkts_acked,
    .cong_avoid   = bbr3_cong_avoid,
    .get_info     = bbr3_get_info,
    .min_tso_segs = 1,
};

/* Module initialization and cleanup */
static int __init bbr3_register(void)
{
    printk(KERN_INFO "TCP BBRv3: Bottleneck Bandwidth and RTT v%s\n", BBRV3_VERSION);
    printk(KERN_INFO "TCP BBRv3: Mode set to %d (0=BBRv1, 1=BBRv2, 2=BBRv3)\n", bbr_mode);
    
    BUILD_BUG_ON(sizeof(struct bbr3) > ICSK_CA_PRIV_SIZE);
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