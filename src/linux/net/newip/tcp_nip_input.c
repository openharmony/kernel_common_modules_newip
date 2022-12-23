// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP INET
 * An implementation of the TCP/IP protocol suite for the LINUX
 * operating system. NewIP INET is implemented using the  BSD Socket
 * interface as the means of communication with the user level.
 *
 * Implementation of the Transmission Control Protocol(TCP).
 *
 * Based on net/ipv4/tcp_input.c
 * Based on net/ipv4/tcp_output.c
 * Based on net/ipv4/tcp_minisocks.c
 */
#define pr_fmt(fmt) "NIP-TCP: " fmt

#include <net/dst.h>
#include <net/tcp.h>
#include <net/tcp_nip.h>
#include <net/inet_common.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/kernel.h>
#include <linux/errqueue.h>
#include "tcp_nip_parameter.h"

#define FLAG_DATA               0x01   /* Incoming frame contained data.           */
#define FLAG_WIN_UPDATE         0x02   /* Incoming ACK was a window update.        */
#define FLAG_DATA_ACKED         0x04   /* This ACK acknowledged new data.          */
#define FLAG_RETRANS_DATA_ACKED 0x08   /* some of which was retransmitted.         */
#define FLAG_SYN_ACKED          0x10   /* This ACK acknowledged SYN.               */
#define FLAG_DATA_SACKED        0x20   /* New SACK.                                */
#define FLAG_ECE                0x40   /* ECE in this ACK                          */
#define FLAG_LOST_RETRANS       0x80   /* This ACK marks some retransmission lost  */
#define FLAG_SLOWPATH           0x100  /* Do not skip RFC checks for window update.*/
#define FLAG_ORIG_SACK_ACKED    0x200  /* Never retransmitted data are (s)acked    */
#define FLAG_SND_UNA_ADVANCED   0x400  /* Snd_una was changed (!= FLAG_DATA_ACKED) */
#define FLAG_DSACKING_ACK       0x800  /* SACK blocks contained D-SACK info        */
#define FLAG_SACK_RENEGING      0x2000 /* snd_una advanced to a sacked seq         */
#define FLAG_UPDATE_TS_RECENT   0x4000 /* tcp_replace_ts_recent()                  */
#define FLAG_NO_CHALLENGE_ACK   0x8000 /* do not call tcp_send_challenge_ack()     */

#define FLAG_ACKED            (FLAG_DATA_ACKED | FLAG_SYN_ACKED)
#define FLAG_NOT_DUP          (FLAG_DATA | FLAG_WIN_UPDATE | FLAG_ACKED)
#define FLAG_CA_ALERT         (FLAG_DATA_SACKED | FLAG_ECE)
#define FLAG_FORWARD_PROGRESS (FLAG_ACKED | FLAG_DATA_SACKED)

#define TCP_REMNANT (TCP_FLAG_FIN | TCP_FLAG_URG | TCP_FLAG_SYN | TCP_FLAG_PSH)
#define TCP_HP_BITS (~(TCP_RESERVED_BITS | TCP_FLAG_PSH))

#define REXMIT_NONE 0 /* no loss recovery to do */
#define REXMIT_LOST 1 /* retransmit packets marked lost */
#define REXMIT_NEW  2 /* FRTO-style transmit of unsent/new packets */

#define TCP_MAX_MSS 1460

void tcp_nip_fin(struct sock *sk)
{
	inet_csk_schedule_ack(sk);

	sk->sk_shutdown |= RCV_SHUTDOWN;
	sock_set_flag(sk, SOCK_DONE);

	switch (sk->sk_state) {
	case TCP_SYN_RECV:
	case TCP_ESTABLISHED:
		/* Move to CLOSE_WAIT */
		tcp_set_state(sk, TCP_CLOSE_WAIT);
		inet_csk(sk)->icsk_ack.pingpong = 1;
		break;

	case TCP_CLOSE_WAIT:
	case TCP_CLOSING:
		/* Received a retransmission of the FIN, do
		 * nothing.
		 */
		break;
	case TCP_LAST_ACK:
		/* RFC793: Remain in the LAST-ACK state. */
		break;

	case TCP_FIN_WAIT1:
		/* This case occurs when a simultaneous close
		 * happens, we must ack the received FIN and
		 * enter the CLOSING state.
		 */
		tcp_nip_send_ack(sk);
		tcp_set_state(sk, TCP_CLOSING);
		break;
	case TCP_FIN_WAIT2:
		/* Received a FIN -- send ACK and enter TIME_WAIT. */
		tcp_nip_send_ack(sk);
		inet_csk_reset_keepalive_timer(sk, TCP_TIMEWAIT_LEN);
		break;
	default:
		/* Only TCP_LISTEN and TCP_CLOSE are left, in these
		 * cases we should never reach this piece of code.
		 */
		nip_dbg("%s: Impossible, sk->sk_state=%d", __func__, sk->sk_state);
		break;
	}

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);
}

static void tcp_nip_overlap_handle(struct tcp_sock *tp, struct sk_buff *skb)
{
		u32 diff = tp->rcv_nxt - TCP_SKB_CB(skb)->seq;
		struct tcp_skb_cb *tcb = TCP_SKB_CB(skb);

		skb->data += diff;
		skb->len -= diff;
		tcb->seq += diff;
}

static void tcp_nip_ofo_queue(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	while (tp->nip_out_of_order_queue) {
		struct sk_buff *skb = tp->nip_out_of_order_queue;

		if (after(TCP_SKB_CB(tp->nip_out_of_order_queue)->seq, tp->rcv_nxt))
			return;
		tp->nip_out_of_order_queue = tp->nip_out_of_order_queue->next;
		skb->next = NULL;
		if (tp->rcv_nxt != TCP_SKB_CB(skb)->seq)
			tcp_nip_overlap_handle(tp, skb);

		__skb_queue_tail(&sk->sk_receive_queue, skb);
		tp->rcv_nxt = TCP_SKB_CB(skb)->end_seq;

		while (tp->nip_out_of_order_queue &&
		       before(TCP_SKB_CB(tp->nip_out_of_order_queue)->end_seq, tp->rcv_nxt)) {
			struct sk_buff *tmp_skb = tp->nip_out_of_order_queue;

			tp->nip_out_of_order_queue = tp->nip_out_of_order_queue->next;
			tmp_skb->next = NULL;
			__kfree_skb(tmp_skb);
		}
	}
}

 /* Maintain a sort list order by the seq. */
static void tcp_nip_data_queue_ofo(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *pre_skb, *cur_skb;

	inet_csk_schedule_ack(sk);
	skb->next = NULL;
	if (!tp->nip_out_of_order_queue) {
		tp->nip_out_of_order_queue = skb;
		skb_set_owner_r(skb, sk);
		return;
	}
	pre_skb = tp->nip_out_of_order_queue;
	cur_skb = pre_skb->next;
	if (TCP_SKB_CB(skb)->seq == TCP_SKB_CB(pre_skb)->seq) {
		if (TCP_SKB_CB(skb)->end_seq > TCP_SKB_CB(pre_skb)->end_seq) {
			skb->next = pre_skb->next;
			pre_skb->next = NULL;
			skb_set_owner_r(skb, sk);
			__kfree_skb(pre_skb);
			return;
		}
		__kfree_skb(skb);
		return;
	} else if (TCP_SKB_CB(skb)->seq < TCP_SKB_CB(pre_skb)->seq) {
		tp->nip_out_of_order_queue = skb;
		skb->next = pre_skb;
		skb_set_owner_r(skb, sk);
		return;
	}
	while (cur_skb) {
		if (TCP_SKB_CB(skb)->seq == TCP_SKB_CB(cur_skb)->seq) {
			/* Same seq, if skb end_seq is bigger, replace. */
			if (TCP_SKB_CB(skb)->end_seq > TCP_SKB_CB(cur_skb)->end_seq) {
				pre_skb->next = skb;
				skb->next = cur_skb->next;
				cur_skb->next = NULL;
				skb_set_owner_r(skb, sk);
				__kfree_skb(cur_skb);
			} else {
				__kfree_skb(skb);
			}
			return;
		} else if (TCP_SKB_CB(skb)->seq < TCP_SKB_CB(cur_skb)->seq) {
			pre_skb->next = skb;
			skb->next = cur_skb;
			skb_set_owner_r(skb, sk);
			return;
		}
		pre_skb = pre_skb->next;
		cur_skb = cur_skb->next;
	}
	pre_skb->next = skb;
	skb_set_owner_r(skb, sk);
}

static void tcp_drop(struct sock *sk, struct sk_buff *skb)
{
	sk_drops_add(sk, skb);
	__kfree_skb(skb);
}

#define PKT_DISCARD_MAX 500
static void tcp_nip_data_queue(struct sock *sk, struct sk_buff *skb)
{
	int mss = tcp_nip_current_mss(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	u32 cur_win = tcp_receive_window(tp);
	u32 seq_max = tp->rcv_nxt + cur_win;

	/* Newip Urg_ptr is disabled. Urg_ptr is used to carry the number of discarded packets */
	tp->snd_up = (TCP_SKB_CB(skb)->seq - tcp_sk(sk)->rcv_nxt) / mss;
	tp->snd_up = tp->snd_up > PKT_DISCARD_MAX ? 0 : tp->snd_up;

	if (TCP_SKB_CB(skb)->seq == TCP_SKB_CB(skb)->end_seq) {
		nip_dbg("%s: no data, only handle ack", __func__);
		__kfree_skb(skb);
		return;
	}

	if (TCP_SKB_CB(skb)->seq == tp->rcv_nxt) {
		if (cur_win == 0) {
			nip_dbg("%s: rcv window is 0", __func__);
			goto out_of_window;
		}
	}

	/* Out of window. F.e. zero window probe. */
	if (!before(TCP_SKB_CB(skb)->seq, seq_max)) {
		nip_dbg("%s: out of rcv win, seq=[%u-%u], rcv_nxt=%u, seq_max=%u",
			__func__, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
			tp->rcv_nxt, seq_max);
		goto out_of_window;
	}

	if (!after(TCP_SKB_CB(skb)->end_seq, tp->rcv_nxt)) {
		/* A retransmit, 2nd most common case.  Force an immediate ack. */
		nip_dbg("%s: rcv retransmit pkt, seq=[%u-%u], rcv_nxt=%u",
			__func__, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq, tp->rcv_nxt);
out_of_window:
		inet_csk_schedule_ack(sk);
		tcp_drop(sk, skb);
		return;
	}
	icsk->icsk_ack.lrcvtime = tcp_jiffies32;
	__skb_pull(skb, tcp_hdr(skb)->doff * TCP_NUM_4);

	if (cur_win == 0 || after(TCP_SKB_CB(skb)->end_seq, seq_max)) {
		nip_dbg("%s: win lack, drop pkt, seq=[%u-%u], seq_max=%u, rmem_alloc/rbuf=[%u:%u]",
			__func__, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
			seq_max, atomic_read(&sk->sk_rmem_alloc), sk->sk_rcvbuf);
		/* wake up processes that are blocked for lack of data */
		sk->sk_data_ready(sk);
		inet_csk_schedule_ack(sk);
		tcp_drop(sk, skb);
		return;
	}

	/* case1: seq == rcv_next
	 * case2: seq -- rcv_next -- end_seq ==> rcv_next(seq) -- end_seq
	 */
	if (TCP_SKB_CB(skb)->seq == tp->rcv_nxt ||
	    (before(TCP_SKB_CB(skb)->seq, tp->rcv_nxt) &&
	     after(TCP_SKB_CB(skb)->end_seq, tp->rcv_nxt))) {
		if (TCP_SKB_CB(skb)->seq != tp->rcv_nxt)
			tcp_nip_overlap_handle(tp, skb);

		nip_dbg("%s: newip packet received. seq=[%u-%u], rcv_nxt=%u, skb->len=%u",
			__func__, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
			tp->rcv_nxt, skb->len);

		__skb_queue_tail(&sk->sk_receive_queue, skb);
		skb_set_owner_r(skb, sk);
		tp->rcv_nxt = TCP_SKB_CB(skb)->end_seq;
		inet_csk_schedule_ack(sk);
		if (TCP_SKB_CB(skb)->tcp_flags & TCPHDR_FIN)
			tcp_nip_fin(sk);
		if (tp->nip_out_of_order_queue)
			tcp_nip_ofo_queue(sk);
		if (!sock_flag(sk, SOCK_DEAD))
			sk->sk_data_ready(sk);
		return;
	}

	nip_dbg("%s: newip ofo packet received. seq=[%u-%u], rcv_nxt=%u, skb->len=%u",
		__func__, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
		tp->rcv_nxt, skb->len);

	tcp_nip_data_queue_ofo(sk, skb);
}

static inline void tcp_nip_push_pending_frames(struct sock *sk)
{
	if (tcp_nip_send_head(sk)) {
		struct tcp_sock *tp = tcp_sk(sk);
		u32 cur_mss = tcp_nip_current_mss(sk);  // TCP_BASE_MSS

		__tcp_nip_push_pending_frames(sk, cur_mss, tp->nonagle);
	}
}

static void tcp_nip_new_space(struct sock *sk)
{
	sk->sk_write_space(sk);
}

static void tcp_nip_check_space(struct sock *sk)
{
	/* Invoke memory barrier (annotated prior to checkpatch requirements) */
	smp_mb();
	if (sk->sk_socket && test_bit(SOCK_NOSPACE, &sk->sk_socket->flags))
		tcp_nip_new_space(sk);
}

static inline void tcp_nip_data_snd_check(struct sock *sk)
{
	tcp_nip_push_pending_frames(sk);
	tcp_nip_check_space(sk);
}

#define TCP_NIP_DELACK_MIN	(HZ / 50)
void tcp_nip_send_delayed_ack(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	int ato = TCP_NIP_DELACK_MIN;
	unsigned long timeout;

	icsk->icsk_ack.ato = TCP_DELACK_MIN;

	/* Stay within the limit we were given */
	timeout = jiffies + ato;

	/* Use new timeout only if there wasn't a older one earlier. */
	if (icsk->icsk_ack.pending & ICSK_ACK_TIMER) {
		if (time_before_eq(icsk->icsk_ack.timeout,
				   jiffies + (ato >> TCP_NIP_4BYTE_PAYLOAD))) {
			nip_dbg("%s: ok", __func__);
			tcp_nip_send_ack(sk);
			return;
		}

		if (!time_before(timeout, icsk->icsk_ack.timeout))
			timeout = icsk->icsk_ack.timeout;
	}

	icsk->icsk_ack.pending |= ICSK_ACK_SCHED | ICSK_ACK_TIMER;
	icsk->icsk_ack.timeout = timeout;
	sk_reset_timer(sk, &icsk->icsk_delack_timer, timeout);
}

static void __tcp_nip_ack_snd_check(struct sock *sk, int ofo_possible)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_nip_common *ntp = &tcp_nip_sk(sk)->common;

	inet_csk(sk)->icsk_ack.rcv_mss = tcp_nip_current_mss(sk); // TCP_BASE_MSS

	/* More than n full frame received... */
	if (((tp->rcv_nxt - tp->rcv_wup) > get_ack_num() * inet_csk(sk)->icsk_ack.rcv_mss &&
	     __nip_tcp_select_window(sk) >= tp->rcv_wnd) ||
	    /* We have out of order data. */
	    (ofo_possible && tp->nip_out_of_order_queue)) {
		if (ofo_possible && tp->nip_out_of_order_queue) {
			if (tp->rcv_nxt == ntp->last_rcv_nxt) {
				ntp->dup_ack_cnt++;
			} else {
				ntp->dup_ack_cnt = 0;
				ntp->last_rcv_nxt = tp->rcv_nxt;
			}
			if (ntp->dup_ack_cnt < get_dup_ack_snd_max())
				tcp_nip_send_ack(sk);
			else if (ntp->dup_ack_cnt % get_dup_ack_snd_max() == 0)
				tcp_nip_send_ack(sk);
		} else {
			tcp_nip_send_ack(sk);
		}
	} else {
		/* Else, send delayed ack. */
		tcp_nip_send_delayed_ack(sk);
	}
}

static inline void tcp_nip_ack_snd_check(struct sock *sk)
{
	if (!inet_csk_ack_scheduled(sk)) {
		/* We sent a data segment already. */
		nip_dbg("We sent a data segment already");
		return;
	}
	__tcp_nip_ack_snd_check(sk, 1);
}

static void tcp_nip_snd_una_update(struct tcp_sock *tp, u32 ack)
{
	u32 delta = ack - tp->snd_una;

	sock_owned_by_me((struct sock *)tp);
	tp->bytes_acked += delta;
	tp->snd_una = ack;
}

void tcp_nip_rearm_rto(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (!tp->packets_out) {
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
	} else {
		u32 rto = inet_csk(sk)->icsk_rto;

		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS, rto, TCP_RTO_MAX);
	}
}

static int tcp_nip_clean_rtx_queue(struct sock *sk, ktime_t *skb_snd_tstamp)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;
	int flag = 0;
	struct inet_connection_sock *icsk = inet_csk(sk);

	while ((skb = tcp_write_queue_head(sk)) && skb != tcp_nip_send_head(sk)) {
		struct tcp_skb_cb *scb = TCP_SKB_CB(skb);
		u32 acked_pcount = 0;

		if (after(scb->end_seq, tp->snd_una)) {
			if (tcp_skb_pcount(skb) == 1 || !after(tp->snd_una, scb->seq))
				break;
			nip_dbg("%s: ack error", __func__);
		} else {
			prefetchw(skb->next);
			acked_pcount = tcp_skb_pcount(skb);
		}

		if (likely(!(scb->tcp_flags & TCPHDR_SYN))) {
			flag |= FLAG_DATA_ACKED;
		} else {
			flag |= FLAG_SYN_ACKED;
			tp->retrans_stamp = 0;
		}

		tp->packets_out -= acked_pcount;

		if (*skb_snd_tstamp == 0)
			*skb_snd_tstamp = skb->tstamp;

		tcp_unlink_write_queue(skb, sk);
		sk_wmem_free_skb(sk, skb);
	}

	icsk->icsk_rto = (unsigned int)(HZ / get_nip_rto());
	if (flag & FLAG_ACKED)
		tcp_nip_rearm_rto(sk);
	return 0;
}

/* Function
 *    Allocate a connection request block that holds connection request information.
 *    At the same time, initialize the set of operations used to send ACK/RST segments
 *    during connection, so that these interfaces can be easily called during establishment.
 *    Set the socket state to TCP_NEW_SYN_RECV
 * Parameter
 *    ops: Request the functional interface of the control block
 *    sk_listener: Transmission control block
 *    attach_listener: Whether to set cookies
 */
struct request_sock *ninet_reqsk_alloc(const struct request_sock_ops *ops,
				       struct sock *sk_listener,
				       bool attach_listener)
{
	struct request_sock *req = reqsk_alloc(ops, sk_listener,
					       attach_listener);

	if (req) {
		struct inet_request_sock *ireq = inet_rsk(req);

		ireq->ireq_opt = NULL;
		atomic64_set(&ireq->ir_cookie, 0);
		ireq->ireq_state = TCP_NEW_SYN_RECV;
		write_pnet(&ireq->ireq_net, sock_net(sk_listener));
		ireq->ireq_family = sk_listener->sk_family;
	}

	return req;
}

static void tcp_nip_drop(struct sock *sk, struct sk_buff *skb)
{
	sk_drops_add(sk, skb);
	__kfree_skb(skb);
}

void tcp_nip_parse_mss(struct tcp_options_received *opt_rx,
		       const struct tcphdr *th,
		       const unsigned char *ptr,
		       int opsize,
		       int estab)
{
	if (opsize == TCPOLEN_MSS && th->syn && !estab) {
		u16 in_mss = get_unaligned_be16(ptr);

		nip_dbg("%s: in_mss %d", __func__, in_mss);

		if (in_mss) {
			if (opt_rx->user_mss &&
			    opt_rx->user_mss < in_mss)
				in_mss = opt_rx->user_mss;
			opt_rx->mss_clamp = in_mss;
		}
	}
}

/* Function
 *    Look for tcp options. Normally only called on SYN and SYNACK packets.
 *    Parsing of TCP options in SKB
 * Parameter
 *    skb: Transfer control block buffer
 *    opt_rx: Saves the structure for TCP options
 *    estab: WANTCOOKIE
 *    foc: Len field
 */
void tcp_nip_parse_options(const struct sk_buff *skb,
			   struct tcp_options_received *opt_rx, int estab,
			   struct tcp_fastopen_cookie *foc)
{
	const unsigned char *ptr;
	const struct tcphdr *th = tcp_hdr(skb);
	/* The length of the TCP option = Length of TCP header - The length of the TCP structure */
	int length = (th->doff * 4) - sizeof(struct tcphdr);

	/* A pointer to the option position */
	ptr = (const unsigned char *)(th + 1);
	opt_rx->saw_tstamp = 0;

	while (length > 0) {
		int opcode = *ptr++;
		int opsize;

		switch (opcode) {
		case TCPOPT_EOL:
			return;
		case TCPOPT_NOP:
			length--;
			continue;
		default:
			opsize = *ptr++;
			if (opsize < 2) /* "2 - silly options" */
				return;
			if (opsize > length)
				return; /* don't parse partial options */
			switch (opcode) {
			case TCPOPT_MSS:
				tcp_nip_parse_mss(opt_rx, th, ptr, opsize, estab);
				break;
			default:
				break;
			}
			ptr += opsize - TCP_NUM_2;
			length -= opsize;
		}
	}
}

static void tcp_nip_common_init(struct request_sock *req)
{
	struct tcp_nip_request_sock *niptreq = tcp_nip_rsk(req);
	struct tcp_nip_common *ntp = &niptreq->common;

	memset(ntp, 0, sizeof(*ntp));
	ntp->nip_ssthresh = get_nip_ssthresh_default();
}

/* Function
 *    Initializes the connection request block information based
 *    on the options and sequence number in the received SYN segment
 * Parameter
 *    req: Request connection control block
 *    rx_opt: Saves the structure for TCP options
 *    skb: Transfer control block buffer.
 *    sk: transmission control block.
 */
static void tcp_nip_openreq_init(struct request_sock *req,
				 const struct tcp_options_received *rx_opt,
				 struct sk_buff *skb, const struct sock *sk)
{
	struct inet_request_sock *ireq = inet_rsk(req);

	tcp_nip_common_init(req);

	req->rsk_rcv_wnd = 0;
	tcp_rsk(req)->rcv_isn = TCP_SKB_CB(skb)->seq;
	tcp_rsk(req)->rcv_nxt = TCP_SKB_CB(skb)->seq + 1;
	tcp_rsk(req)->snt_synack = tcp_clock_us();
	tcp_rsk(req)->last_oow_ack_time = 0;
	req->mss = rx_opt->mss_clamp;
	req->ts_recent = rx_opt->saw_tstamp ? rx_opt->rcv_tsval : 0;
	ireq->tstamp_ok = rx_opt->tstamp_ok;
	ireq->snd_wscale = rx_opt->snd_wscale;

	if (get_wscale_enable()) {
		ireq->wscale_ok = 1;
		ireq->snd_wscale = get_wscale(); // rx_opt->snd_wscale;
		ireq->rcv_wscale = get_wscale();
	}

	ireq->acked = 0;
	ireq->ecn_ok = 0;
	ireq->ir_rmt_port = tcp_hdr(skb)->source;
	ireq->ir_num = ntohs(tcp_hdr(skb)->dest);
	ireq->ir_mark = sk->sk_mark;
}

/* Function
 *    Based on listening SOCK and REQ, create a transport control block
 *    for the new connection and initialize it.
 * Parameter
 *    sk: the listening transmission control block.
 *    req: Request connection control block
 *    skb: Transfer control block buffer.
 */
struct sock *tcp_nip_create_openreq_child(const struct sock *sk,
					  struct request_sock *req,
					  struct sk_buff *skb)
{
	/* Clone a transport control block and lock the new transport control block */
	struct sock *newsk = inet_csk_clone_lock(sk, req, GFP_ATOMIC);

	if (newsk) {
		const struct inet_request_sock *ireq = inet_rsk(req);
		struct tcp_request_sock *treq = tcp_rsk(req);
		struct inet_connection_sock *newicsk = inet_csk(newsk);
		struct tcp_sock *newtp = tcp_sk(newsk);

		/* Now setup tcp_sock */
		newtp->pred_flags = 0;

		/* The variables related to the receiving and sending serial numbers
		 * are initialized. The second handshake sends an ACK in the SYN+ACK segment
		 */
		newtp->rcv_wup = treq->rcv_isn + 1;
		newtp->copied_seq = treq->rcv_isn + 1;
		newtp->rcv_nxt = treq->rcv_isn + 1;
		newtp->segs_in = 1;
		/* The second handshake sends seq+1 in the SYN+ACK segment */
		newtp->snd_sml = treq->snt_isn + 1;
		newtp->snd_una = treq->snt_isn + 1;
		newtp->snd_nxt = treq->snt_isn + 1;
		newtp->snd_up = treq->snt_isn + 1;

		INIT_LIST_HEAD(&newtp->tsq_node);

		/* The ACK segment number of the send window that
		 * received the first handshake update
		 */
		tcp_init_wl(newtp, treq->rcv_isn);

		/* Initialization of delay-related variables */
		minmax_reset(&newtp->rtt_min, tcp_jiffies32, ~0U);
		newicsk->icsk_rto = get_nip_rto() == 0 ? TCP_TIMEOUT_INIT : (HZ / get_nip_rto());
		newicsk->icsk_ack.lrcvtime = tcp_jiffies32;

		/* The congestion control-related variables are initialized */
		newtp->packets_out = 0;

		newtp->snd_ssthresh = TCP_INFINITE_SSTHRESH;

		newtp->lsndtime = tcp_jiffies32;

		newtp->total_retrans = req->num_retrans;

		newtp->snd_cwnd = TCP_INIT_CWND;

		/* There's a bubble in the pipe until at least the first ACK. */
		newtp->app_limited = ~0U;

		/* Initialize several timers */
		tcp_nip_init_xmit_timers(newsk);
		newtp->write_seq = treq->snt_isn + 1;
		newtp->pushed_seq = treq->snt_isn + 1;

		/* TCP option correlation */
		newtp->rx_opt.saw_tstamp = 0;

		newtp->rx_opt.dsack = 0;
		newtp->rx_opt.num_sacks = 0;

		newtp->urg_data = 0;

		newtp->rx_opt.tstamp_ok = ireq->tstamp_ok;
		newtp->window_clamp = req->rsk_window_clamp;
		newtp->rcv_ssthresh = req->rsk_rcv_wnd;
		newtp->rcv_wnd = req->rsk_rcv_wnd;
		newtp->rx_opt.wscale_ok = ireq->wscale_ok;
		if (newtp->rx_opt.wscale_ok) {
			newtp->rx_opt.snd_wscale = ireq->snd_wscale;
			newtp->rx_opt.rcv_wscale = ireq->rcv_wscale;
		} else {
			newtp->rx_opt.snd_wscale = 0;
			newtp->rx_opt.rcv_wscale = 0;
			newtp->window_clamp = min(newtp->window_clamp, 65535U);
		}
		newtp->snd_wnd = (ntohs(tcp_hdr(skb)->window) <<
				  newtp->rx_opt.snd_wscale);
		newtp->max_window = newtp->snd_wnd;

		if (newtp->rx_opt.tstamp_ok) {
			newtp->rx_opt.ts_recent = req->ts_recent;
			newtp->rx_opt.ts_recent_stamp = get_seconds();
			newtp->tcp_header_len = sizeof(struct tcphdr) + TCPOLEN_TSTAMP_ALIGNED;
		} else {
			newtp->rx_opt.ts_recent_stamp = 0;
			newtp->tcp_header_len = sizeof(struct tcphdr);
		}
		newtp->tsoffset = 0;

		/* Determines the size of the last passed segment */
		if (skb->len >= TCP_MSS_DEFAULT + newtp->tcp_header_len)
			newicsk->icsk_ack.last_seg_size = skb->len - newtp->tcp_header_len;
		newtp->rx_opt.mss_clamp = req->mss;
		newtp->fastopen_req = NULL;
		newtp->fastopen_rsk = NULL;
		newtp->syn_data_acked = 0;
		newtp->rack.mstamp = 0;
		newtp->rack.advanced = 0;

		__TCP_INC_STATS(sock_net(sk), TCP_MIB_PASSIVEOPENS);
	}
	return newsk;
}

void tcp_nip_openreq_init_rwin(struct request_sock *req,
			       const struct sock *sk_listener,
			       const struct dst_entry *dst)
{
	struct inet_request_sock *ireq = inet_rsk(req);
	const struct tcp_sock *tp = tcp_sk(sk_listener);
	int full_space = tcp_full_space(sk_listener);
	int mss;
	u32 window_clamp;
	__u8 rcv_wscale;

	mss = tcp_mss_clamp(tp, dst_metric_advmss(dst));

	window_clamp = READ_ONCE(tp->window_clamp);
	/* Set this up on the first call only */
	req->rsk_window_clamp = window_clamp ? : dst_metric(dst, RTAX_WINDOW);

	/* limit the window selection if the user enforce a smaller rx buffer */
	if (sk_listener->sk_userlocks & SOCK_RCVBUF_LOCK &&
	    (req->rsk_window_clamp > full_space || req->rsk_window_clamp == 0))
		req->rsk_window_clamp = full_space;

	/* tcp_full_space because it is guaranteed to be the first packet */
	tcp_select_initial_window(sk_listener, full_space,
				  mss - (ireq->tstamp_ok ? TCPOLEN_TSTAMP_ALIGNED : 0),
				  &req->rsk_rcv_wnd,
				  &req->rsk_window_clamp,
				  0,
				  &rcv_wscale,
				  0);
	ireq->rcv_wscale = get_wscale_enable() ? get_wscale() : rcv_wscale;
}

/* Function
 *    A function used by the server to process client connection requests.
 * Parameter
 *    rsk_ops: Functional interface to request control blocks.
 *    af_ops: The functional interface of the TCP request block.
 *    sk: transmission control block.
 *    skb: Transfer control block buffer.
 */
int _tcp_nip_conn_request(struct request_sock_ops *rsk_ops,
			  const struct tcp_request_sock_ops *af_ops,
			  struct sock *sk, struct sk_buff *skb)
{
	struct tcp_fastopen_cookie foc = { .len = -1 };

	__u32 isn = TCP_SKB_CB(skb)->tcp_tw_isn;
	/* All received TCP options are resolved into this structure */
	struct tcp_options_received tmp_opt;
	struct tcp_sock *tp = tcp_sk(sk);
	struct dst_entry *dst = NULL;
	struct request_sock *req;

	/* If the half-connection queue length has reached the upper limit,
	 * the current request is discarded
	 */
	if (inet_csk_reqsk_queue_is_full(sk) && !isn) {
		nip_dbg("inet_csk_reqsk_queue_is_full");
		goto drop;
	}

	/* If the queue holds the socket that has completed the connection (full connection queue)
	 * The length has reached its upper limit
	 * The current request is discarded
	 */
	if (sk_acceptq_is_full(sk)) {
		NET_INC_STATS(sock_net(sk), LINUX_MIB_LISTENOVERFLOWS);
		nip_dbg("%s sk_acceptq_is_full, sk_ack_backlog=%u, sk_max_ack_backlog=%u",
			__func__, READ_ONCE(sk->sk_ack_backlog),
			READ_ONCE(sk->sk_max_ack_backlog));
		goto drop;
	}

	/* Allocate a connection request block that holds connection request information
	 * While initializing the connection process
	 * The set of operations that send ACK/RST segments
	 * These interfaces can be easily invoked during the setup process.
	 */
	req = ninet_reqsk_alloc(rsk_ops, sk, true);
	if (!req)
		goto drop;

	tcp_rsk(req)->af_specific = af_ops;

	tcp_clear_options(&tmp_opt);
	/* Maximum MSS negotiated during connection establishment */
	tmp_opt.mss_clamp = af_ops->mss_clamp;
	/* The best way to do this is to prink the value of user_mss and see if it is 0 */
	tmp_opt.user_mss  = tp->rx_opt.user_mss;
	/* Parsing of TCP options in SKB */
	tcp_nip_parse_options(skb, &tmp_opt, 0, NULL);

	/* Tstamp_ok indicates the TIMESTAMP seen on the received SYN packet */
	tmp_opt.tstamp_ok = tmp_opt.saw_tstamp;
	/* Initializes the connection request block information based on the options
	 * and sequence number in the received SYN segment
	 */
	tcp_nip_openreq_init(req, &tmp_opt, skb, sk);

	inet_rsk(req)->ir_iif = sk->sk_bound_dev_if;

	af_ops->init_req(req, sk, skb);

	if (!isn)
		isn = af_ops->init_seq(skb);

	if (!dst) {
		dst = af_ops->route_req(sk, NULL, req);
		if (!dst)
			goto drop_and_free;
	}

	tcp_rsk(req)->snt_isn = isn;
	tcp_rsk(req)->txhash = net_tx_rndhash();
	/* Initialize the receive window */
	tcp_nip_openreq_init_rwin(req, sk, dst);
	/* Record the syn */
	tcp_rsk(req)->tfo_listener = false;
	/* Add a timer to add reQ to the ehash table */
	ninet_csk_reqsk_queue_hash_add(sk, req, TCP_TIMEOUT_INIT);

	af_ops->send_synack(sk, dst, NULL, req, &foc, TCP_SYNACK_NORMAL, NULL);

	reqsk_put(req);
	return 0;

drop_and_free:
	reqsk_free(req);
drop:
	tcp_listendrop(sk);
	return 0;
}

static inline bool tcp_nip_paws_check(const struct tcp_options_received *rx_opt,
				      int paws_win)
{
	if ((s32)(rx_opt->ts_recent - rx_opt->rcv_tsval) <= paws_win)
		return true;
	if (unlikely(get_seconds() >= rx_opt->ts_recent_stamp + TCP_PAWS_24DAYS))
		return true;

	if (!rx_opt->ts_recent)
		return true;
	return false;
}

static inline bool tcp_nip_may_update_window(const struct tcp_sock *tp,
					     const u32 ack, const u32 ack_seq,
					     const u32 nwin)
{
	return	after(ack, tp->snd_una) ||
		after(ack_seq, tp->snd_wl1) ||
		(ack_seq == tp->snd_wl1 && nwin > tp->snd_wnd);
}

static void tcp_nip_ack_update_window(struct sock *sk, const struct sk_buff *skb, u32 ack,
				      u32 ack_seq)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u32 nwin = ntohs(tcp_hdr(skb)->window);

	if (likely(!tcp_hdr(skb)->syn))
		nwin <<= tp->rx_opt.snd_wscale;

	if (tcp_nip_may_update_window(tp, ack, ack_seq, nwin)) {
		tcp_update_wl(tp, ack_seq);

		if (tp->snd_wnd != nwin) {
			nip_dbg("%s snd_wnd change [%u to %u]", __func__, tp->snd_wnd, nwin);
			tp->snd_wnd = nwin;
			tp->pred_flags = 0;
		}
	}
}

/* Check whether the ACK returned by the packet is detected
 * and whether the peer window is opened
 */
static void tcp_nip_ack_probe(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	if (!after(TCP_SKB_CB(tcp_nip_send_head(sk))->end_seq, tcp_wnd_end(tp))) {
		icsk->icsk_backoff = 0;
		nip_dbg("%s stop probe0 timer", __func__);
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_PROBE0);
		/* Socket must be waked up by subsequent tcp_data_snd_check().
		 * This function is not for random using!
		 */
	} else {
		unsigned long when = tcp_probe0_when(sk, TCP_RTO_MAX);
		unsigned long base_when = tcp_probe0_base(sk);
		u8 icsk_backoff = inet_csk(sk)->icsk_backoff;

		nip_dbg("%s start probe0 timer, when=%u, RTO MAX=%u, base_when=%u, backoff=%u",
			__func__, when, TCP_RTO_MAX, base_when, icsk_backoff);
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_PROBE0, when, TCP_RTO_MAX);
	}
}

#define DUP_ACK 0
#define NOR_ACK 1
#define ACK_DEF 2
static void tcp_nip_ack_retrans(struct sock *sk, u32 ack, int ack_type, u32 retrans_num)
{
	int skb_index = 0;
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_nip_common *ntp = &tcp_nip_sk(sk)->common;
	struct sk_buff *skb, *tmp;
	const char *ack_str[ACK_DEF] = {"dup", "nor"};
	int index = ack_type == DUP_ACK ? DUP_ACK : NOR_ACK;

	skb_queue_walk_safe(&sk->sk_write_queue, skb, tmp) {
		if (skb == tcp_nip_send_head(sk)) {
			ssthresh_dbg("%s %s ack retrans(%u) end, ack=%u, seq=%u~%u, pkt_out=%u",
				     __func__, ack_str[index], ntp->ack_retrans_num, ack,
				     tp->selective_acks[0].start_seq,
				     tp->selective_acks[0].end_seq, tp->packets_out);
			tp->selective_acks[0].start_seq = 0;
			tp->selective_acks[0].end_seq = 0;
			ntp->ack_retrans_seq = 0;
			ntp->ack_retrans_num = 0;
			break;
		}

		if (TCP_SKB_CB(skb)->seq > tp->selective_acks[0].end_seq) {
			ssthresh_dbg("%s %s ack retrans(%u) finish, ack=%u, seq=%u~%u, pkt_out=%u",
				     __func__, ack_str[index], ntp->ack_retrans_num, ack,
				     tp->selective_acks[0].start_seq,
				     tp->selective_acks[0].end_seq, tp->packets_out);

			tp->selective_acks[0].start_seq = 0;
			tp->selective_acks[0].end_seq = 0;
			ntp->ack_retrans_seq = 0;
			ntp->ack_retrans_num = 0;
			break;
		}

		if (TCP_SKB_CB(skb)->seq != ntp->ack_retrans_seq)
			continue;

		if (skb_index < retrans_num) {
			tcp_nip_retransmit_skb(sk, skb, 1);
			skb_index++;
			ntp->ack_retrans_num++;
			ntp->ack_retrans_seq = TCP_SKB_CB(skb)->end_seq;
		} else {
			retrans_dbg("%s %s ack retrans(%u) no end, ack=%u, seq=%u~%u, pkt_out=%u",
				    __func__, ack_str[index], ntp->ack_retrans_num, ack,
				    tp->selective_acks[0].start_seq,
				    tp->selective_acks[0].end_seq, tp->packets_out);
			break;
		}
	}
}

#define DUP_ACK_RETRANS_START_NUM 3
#define DIVIDEND_UP 3
#define DIVIDEND_DOWN 5
static void tcp_nip_dup_ack_retrans(struct sock *sk, const struct sk_buff *skb,
				    u32 ack, u32 retrans_num)
{
	if (tcp_write_queue_head(sk)) {
		struct tcp_sock *tp = tcp_sk(sk);
		struct tcp_nip_common *ntp = &tcp_nip_sk(sk)->common;

		tp->sacked_out++;
		if (tp->sacked_out == DUP_ACK_RETRANS_START_NUM) {
			/* Newip Urg_ptr is disabled. Urg_ptr is used to
			 * carry the number of discarded packets
			 */
			int mss = tcp_nip_current_mss(sk);
			struct tcphdr *th = (struct tcphdr *)skb->data;
			u16 discard_num = htons(th->urg_ptr);
			u32 last_nip_ssthresh = ntp->nip_ssthresh;

			if (tp->selective_acks[0].end_seq)
				ssthresh_dbg("%s last retans(%u) not end, seq=%u~%u, pkt_out=%u",
					     __func__, ntp->ack_retrans_num,
					     tp->selective_acks[0].start_seq,
					     tp->selective_acks[0].end_seq,
					     tp->packets_out);

			tp->selective_acks[0].start_seq = ack;
			tp->selective_acks[0].end_seq = ack + discard_num * mss;
			ntp->ack_retrans_seq = ack;
			ntp->ack_retrans_num = 0;

			ntp->nip_ssthresh = get_ssthresh_low();
			ssthresh_dbg("%s new dup ack, win %u to %u, discard_num=%u, seq=%u~%u",
				     __func__, last_nip_ssthresh, ntp->nip_ssthresh, discard_num,
				     tp->selective_acks[0].start_seq,
				     tp->selective_acks[0].end_seq);

			tcp_nip_ack_retrans(sk, ack, DUP_ACK, retrans_num);
		}
	}
}

static void tcp_nip_nor_ack_retrans(struct sock *sk, u32 ack, u32 retrans_num)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_nip_common *ntp = &tcp_nip_sk(sk)->common;

	if (tp->selective_acks[0].end_seq != 0) {
		if (ack >= tp->selective_acks[0].end_seq) {
			ssthresh_dbg("%s nor ack retrans(%u) resume, seq=%u~%u, pkt_out=%u, ack=%u",
				     __func__, ntp->ack_retrans_num,
				     tp->selective_acks[0].start_seq,
				     tp->selective_acks[0].end_seq, tp->packets_out, ack);
			tp->selective_acks[0].start_seq = 0;
			tp->selective_acks[0].end_seq = 0;
			ntp->ack_retrans_seq = 0;
			ntp->ack_retrans_num = 0;

			tp->sacked_out = 0;
			return;
		}

		tcp_nip_ack_retrans(sk, ack, NOR_ACK, retrans_num);
	}

	tp->sacked_out = 0;
}

static void tcp_nip_ack_calc_ssthresh(struct sock *sk, u32 ack, int icsk_rto_last,
				      ktime_t skb_snd_tstamp)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_nip_common *ntp = &tcp_nip_sk(sk)->common;
	struct inet_connection_sock *icsk = inet_csk(sk);
	int ack_reset = ack / get_nip_ssthresh_reset();
	u32 nip_ssthresh;

	if (ntp->nip_ssthresh_reset != ack_reset) {
		ssthresh_dbg("%s ack reset win %u to %u, ack=%u",
			     __func__, ntp->nip_ssthresh, get_ssthresh_low(), ack);
		ntp->nip_ssthresh_reset = ack_reset;
		ntp->nip_ssthresh = get_ssthresh_low();
	} else {
		if (skb_snd_tstamp) {
			u32 rtt_tstamp = tp->rcv_tstamp - skb_snd_tstamp;

			if (rtt_tstamp >= get_rtt_tstamp_rto_up()) {
				ssthresh_dbg("%s rtt %u >= %u, win %u to %u, rto %u to %u, ack=%u",
					     __func__, rtt_tstamp, get_rtt_tstamp_rto_up(),
					     ntp->nip_ssthresh, get_ssthresh_low_min(),
					     icsk_rto_last, icsk->icsk_rto, ack);

				ntp->nip_ssthresh = get_ssthresh_low_min();
			} else if (rtt_tstamp >= get_rtt_tstamp_high()) {
				ssthresh_dbg("%s rtt %u >= %u, win %u to %u, ack=%u",
					     __func__, rtt_tstamp, get_rtt_tstamp_high(),
					     ntp->nip_ssthresh, get_ssthresh_low(), ack);

				ntp->nip_ssthresh = get_ssthresh_low();
			} else if (rtt_tstamp >= get_rtt_tstamp_mid_high()) {
				ssthresh_dbg("%s rtt %u >= %u, win %u to %u, ack=%u",
					     __func__, rtt_tstamp, get_rtt_tstamp_mid_high(),
					     ntp->nip_ssthresh, get_ssthresh_mid_low(), ack);

				ntp->nip_ssthresh = get_ssthresh_mid_low();
			} else if (rtt_tstamp >= get_rtt_tstamp_mid_low()) {
				u32 rtt_tstamp_scale = get_rtt_tstamp_mid_high() - rtt_tstamp;
				int half_mid_high = get_ssthresh_mid_high() / 2;

				nip_ssthresh = half_mid_high + rtt_tstamp_scale * half_mid_high /
					       (get_rtt_tstamp_mid_high() -
					       get_rtt_tstamp_mid_low());

				ntp->nip_ssthresh = ntp->nip_ssthresh > get_ssthresh_mid_high() ?
						    half_mid_high : ntp->nip_ssthresh;
				nip_ssthresh = (ntp->nip_ssthresh * get_ssthresh_high_step() +
					       nip_ssthresh) / (get_ssthresh_high_step() + 1);

				ssthresh_dbg("%s rtt %u >= %u, win %u to %u, ack=%u",
					     __func__, rtt_tstamp, get_rtt_tstamp_mid_low(),
					     ntp->nip_ssthresh, nip_ssthresh, ack);

				ntp->nip_ssthresh = nip_ssthresh;
			} else if (rtt_tstamp != 0) {
				nip_ssthresh = (ntp->nip_ssthresh * get_ssthresh_high_step() +
					       get_ssthresh_high()) /
					       (get_ssthresh_high_step() + 1);

				ssthresh_dbg("%s rtt %u < %u, win %u to %u, ack=%u",
					     __func__, rtt_tstamp, get_rtt_tstamp_mid_low(),
					     ntp->nip_ssthresh, nip_ssthresh, ack);

				ntp->nip_ssthresh =  nip_ssthresh;
			}
		}
	}
}

static int tcp_nip_ack(struct sock *sk, const struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_nip_common *ntp = &tcp_nip_sk(sk)->common;
	struct inet_connection_sock *icsk = inet_csk(sk);
	u32 prior_snd_una = tp->snd_una;
	u32 ack_seq = TCP_SKB_CB(skb)->seq;
	u32 ack = TCP_SKB_CB(skb)->ack_seq;
	int prior_packets = tp->packets_out;
	ktime_t skb_snd_tstamp = 0;

	if (before(ack, prior_snd_una))
		return 0;
	if (after(ack, tp->snd_nxt))
		return -1;

	tcp_nip_ack_update_window(sk, skb, ack, ack_seq);
	icsk->icsk_probes_out = 0;  /* probe0 cnt */
	ntp->nip_keepalive_out = 0; /* keepalive cnt */
	tp->rcv_tstamp = tcp_jiffies32;

	/* maybe zero window probe */
	if (!prior_packets) {
		nip_dbg("%s: no unack pkt, seq=[%u-%u], rcv_nxt=%u, ack=%u", __func__,
			TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq, tp->rcv_nxt, ack);
		if (tcp_nip_send_head(sk))
			tcp_nip_ack_probe(sk);
		return 1;
	}

	if (after(ack, prior_snd_una)) {
		int icsk_rto_last;

		icsk->icsk_retransmits = 0;
		tp->retrans_stamp = tcp_time_stamp(tp);
		tp->rcv_tstamp = tcp_jiffies32;
		tcp_nip_snd_una_update(tp, ack);

		icsk_rto_last = icsk->icsk_rto;
		tcp_nip_clean_rtx_queue(sk, &skb_snd_tstamp);

		tcp_nip_ack_calc_ssthresh(sk, ack, icsk_rto_last, skb_snd_tstamp);
		tcp_nip_nor_ack_retrans(sk, ack, get_ack_retrans_num());
		return 1;
	}

	// dup ack: ack == tp->snd_una
	tcp_nip_dup_ack_retrans(sk, skb, ack, get_dup_ack_retrans_num());

	return 1;
}

static inline bool tcp_nip_sequence(const struct tcp_sock *tp, u32 seq, u32 end_seq)
{
	/* False is returned if end_seq has been received,
	 * or if SEq is not behind the receive window
	 */
	return	!before(end_seq, tp->rcv_wup) &&
		!after(seq, tp->rcv_nxt + tcp_receive_window(tp));
}

/* When we get a reset we do this. */
void tcp_nip_reset(struct sock *sk)
{
	nip_dbg("%s: handle RST", __func__);

	/* We want the right error as BSD sees it (and indeed as we do). */
	switch (sk->sk_state) {
	case TCP_SYN_SENT:
		sk->sk_err = ECONNREFUSED;
		break;
	case TCP_CLOSE_WAIT:
		sk->sk_err = EPIPE;
		break;
	case TCP_CLOSE:
		return;
	default:
		sk->sk_err = ECONNRESET;
	}
	/* This barrier is coupled with smp_rmb() in tcp_poll() */
	smp_wmb();

	tcp_nip_write_queue_purge(sk);
	tcp_nip_done(sk);

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_error_report(sk);
}

/* Reack some incorrect packets, because if you do not ACK these packets,
 * they may be retransmitted frequently
 */
static void tcp_nip_send_dupack(struct sock *sk, const struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (TCP_SKB_CB(skb)->end_seq != TCP_SKB_CB(skb)->seq &&
	    before(TCP_SKB_CB(skb)->seq, tp->rcv_nxt))
		NET_INC_STATS(sock_net(sk), LINUX_MIB_DELAYEDACKLOST);

	nip_dbg("%s send dup ack", __func__);
	tcp_nip_send_ack(sk);
}

static bool tcp_nip_reset_check(const struct sock *sk, const struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);

	return unlikely(TCP_SKB_CB(skb)->seq == (tp->rcv_nxt - 1) &&
			(1 << sk->sk_state) & (TCPF_CLOSE_WAIT | TCPF_LAST_ACK |
					       TCPF_CLOSING));
}

/* This function is used to process the SYN received in RST packets
 * and illegal SEQ packets in ESTABLISHED state. Currently only seQ checks are included
 */
static bool tcp_nip_validate_incoming(struct sock *sk, struct sk_buff *skb,
				      const struct tcphdr *th, int syn_inerr)
{
	struct tcp_sock *tp = tcp_sk(sk);
	bool rst_seq_match = false;

	/* Step 1: check sequence number */
	/* 01.Check for unexpected packets. For some probe packets,
	 *    unexpected packets do not need to be processed, but reply for an ACK.
	 * 02.Enter this branch when the receive window is 0
	 */
	if (!tcp_nip_sequence(tp, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq)) {
		nip_dbg("%s receive unexpected pkt, drop it. seq=[%u-%u], rec_win=[%u-%u]",
			__func__, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
			tp->rcv_wup, tp->rcv_nxt + tcp_receive_window(tp));
		if (!th->rst)
			tcp_nip_send_dupack(sk, skb);
		else if (tcp_nip_reset_check(sk, skb))
			tcp_nip_reset(sk);
		goto discard;
	}

	/* Step 2: check RST bit */
	if (th->rst) {
		if (TCP_SKB_CB(skb)->seq == tp->rcv_nxt || tcp_nip_reset_check(sk, skb))
			rst_seq_match = true;
		if (rst_seq_match)
			tcp_nip_reset(sk);
		goto discard;
	}

	return true;

discard:
	tcp_drop(sk, skb);
	return false;
}

void tcp_nip_rcv_established(struct sock *sk, struct sk_buff *skb,
			     const struct tcphdr *th, unsigned int len)
{
	struct tcp_sock *tp = tcp_sk(sk);

	tcp_mstamp_refresh(tp);
	if (unlikely(!sk->sk_rx_dst))
		inet_csk(sk)->icsk_af_ops->sk_rx_dst_set(sk, skb);

	if (!tcp_nip_validate_incoming(sk, skb, th, 1))
		return;

	if (tcp_nip_ack(sk, skb) < 0)
		goto discard;

	tcp_nip_data_queue(sk, skb);
	tcp_nip_data_snd_check(sk);
	tcp_nip_ack_snd_check(sk);

	return;

discard:
	tcp_drop(sk, skb);
}

static u32 tcp_default_init_rwnd(u32 mss)
{
	u32 init_rwnd = TCP_INIT_CWND * 2;

	if (mss > TCP_MAX_MSS)
		init_rwnd = max((TCP_MAX_MSS * init_rwnd) / mss, 2U);
	return init_rwnd;
}

static void tcp_nip_fixup_rcvbuf(struct sock *sk)
{
	u32 mss = TCP_BASE_MSS;
	int rcvmem;

	rcvmem = TCP_NUM_2 * SKB_TRUESIZE(mss + MAX_TCP_HEADER) *
		 tcp_default_init_rwnd(mss);

	if (sock_net(sk)->ipv4.sysctl_tcp_moderate_rcvbuf)
		rcvmem <<= TCP_NIP_4BYTE_PAYLOAD;

	if (sk->sk_rcvbuf < rcvmem)
		sk->sk_rcvbuf = min(rcvmem,
				    sock_net(sk)->ipv4.sysctl_tcp_rmem[TCP_ARRAY_INDEX_2]);
}

#define TCP_NIP_SND_BUF_SIZE 30720
void tcp_nip_init_buffer_space(struct sock *sk)
{
	int tcp_app_win = sock_net(sk)->ipv4.sysctl_tcp_app_win;
	struct tcp_sock *tp = tcp_sk(sk);
	int maxwin;

	if (!(sk->sk_userlocks & SOCK_RCVBUF_LOCK))
		tcp_nip_fixup_rcvbuf(sk);

	tp->rcvq_space.space = tp->rcv_wnd;
	tcp_mstamp_refresh(tp);
	tp->rcvq_space.time = jiffies;
	tp->rcvq_space.seq = tp->copied_seq;
	maxwin = tcp_full_space(sk);
	if (tp->window_clamp >= maxwin) {
		tp->window_clamp = maxwin;
		if (tcp_app_win && maxwin > TCP_NUM_4 * tp->advmss)
			tp->window_clamp = max(maxwin -
					       (maxwin >> tcp_app_win),
					       TCP_NUM_4 * tp->advmss);
	}
	/* Force reservation of one segment. */
	if (tcp_app_win &&
	    tp->window_clamp > TCP_NUM_2 * tp->advmss &&
	    tp->window_clamp + tp->advmss > maxwin)
		tp->window_clamp = max(TCP_NUM_2 * tp->advmss, maxwin - tp->advmss);
	tp->rcv_ssthresh = min(tp->rcv_ssthresh, tp->window_clamp);
	tp->snd_cwnd_stamp = tcp_jiffies32;
}

void tcp_nip_finish_connect(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	tcp_set_state(sk, TCP_ESTABLISHED);
	icsk->icsk_ack.lrcvtime = tcp_jiffies32;
	if (skb) {
		icsk->icsk_af_ops->sk_rx_dst_set(sk, skb);
		security_inet_conn_established(sk, skb);
	}

	tp->lsndtime = tcp_jiffies32;

	tcp_nip_init_buffer_space(sk);
}

/* Function:
 *    A function that handles the second handshake
 * Parameter：
 *    sk: transmission control block
 *    skb: Transfer control block buffer
 *    Th: TCP header field
 */
static int tcp_nip_rcv_synsent_state_process(struct sock *sk, struct sk_buff *skb,
					     const struct tcphdr *th)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int saved_clamp = tp->rx_opt.mss_clamp;

	/* TCP Option Parsing */
	tcp_nip_parse_options(skb, &tp->rx_opt, 0, NULL);
	/* Rcv_tsecr saves the timestamp of the last TCP segment received from the peer end */
	if (tp->rx_opt.saw_tstamp && tp->rx_opt.rcv_tsecr)
		tp->rx_opt.rcv_tsecr -= tp->tsoffset;

	if (th->ack) {
		/* Whether the ACK value is between the initial send sequence number
		 * and the next sequence number
		 */
		if (!after(TCP_SKB_CB(skb)->ack_seq, tp->snd_una) ||
		    after(TCP_SKB_CB(skb)->ack_seq, tp->snd_nxt))
			goto reset_and_undo;
		/* Must be within the corresponding time */
		if (tp->rx_opt.saw_tstamp && tp->rx_opt.rcv_tsecr &&
		    !between(tp->rx_opt.rcv_tsecr, tp->retrans_stamp, tcp_time_stamp(tp))) {
			NET_INC_STATS(sock_net(sk), LINUX_MIB_PAWSACTIVEREJECTED);
			goto reset_and_undo;
		}

		if (th->rst) {
			tcp_nip_reset(sk);
			goto discard;
		}

		if (!th->syn)
			goto discard_and_undo;

		tcp_init_wl(tp, TCP_SKB_CB(skb)->seq);

		tcp_nip_ack(sk, skb);
		tp->nip_out_of_order_queue = NULL;
		/* The next data number expected to be accepted is +1 */
		tp->rcv_nxt = TCP_SKB_CB(skb)->seq + 1;
		/* Accept the left margin of the window +1 */
		tp->rcv_wup = TCP_SKB_CB(skb)->seq + 1;
		tp->snd_wnd = ntohs(th->window);

		if (get_wscale_enable()) {
			tp->rx_opt.wscale_ok = 1;
			tp->rx_opt.snd_wscale = get_wscale();
			tp->rx_opt.rcv_wscale = get_wscale();
		}

		if (!tp->rx_opt.wscale_ok) {
			tp->rx_opt.snd_wscale = 0;
			tp->rx_opt.rcv_wscale = 0;
			tp->window_clamp = min(tp->window_clamp, 65535U);
		}

		if (tp->rx_opt.saw_tstamp) {
			tp->rx_opt.tstamp_ok	   = 1;
			tp->tcp_header_len =
			sizeof(struct tcphdr) + TCPOLEN_TSTAMP_ALIGNED;
			tp->advmss	    -= TCPOLEN_TSTAMP_ALIGNED;
			tp->rx_opt.ts_recent = tp->rx_opt.rcv_tsval;
			tp->rx_opt.ts_recent_stamp = get_seconds();
		} else {
			tp->tcp_header_len = sizeof(struct tcphdr);
		}

		tp->copied_seq = tp->rcv_nxt;
		/* Invoke memory barrier (annotated prior to checkpatch requirements) */
		smp_mb();

		tcp_nip_sync_mss(sk, icsk->icsk_pmtu_cookie);
		tcp_nip_initialize_rcv_mss(sk);

		tcp_nip_finish_connect(sk, skb);
		/* Wake up the process */
		if (!sock_flag(sk, SOCK_DEAD)) {
			sk->sk_state_change(sk);
			rcu_read_lock();
			sock_wake_async(rcu_dereference(sk->sk_wq), SOCK_WAKE_IO, POLL_OUT);
			rcu_read_unlock();
		}

		tcp_nip_send_ack(sk);
		return -1;
discard:
		tcp_drop(sk, skb);
		return 0;
	}

discard_and_undo:
	tcp_clear_options(&tp->rx_opt);
	tp->rx_opt.mss_clamp = saved_clamp;
	goto discard;

reset_and_undo:
	tcp_clear_options(&tp->rx_opt);
	tp->rx_opt.mss_clamp = saved_clamp;
	return 1;
}

/* Function:
 *    TCP processing function that is differentiated according to
 *    different states after receiving data packets
 * Parameter：
 *    sk: transmission control block
 *    skb: Transfer control block buffer
 * Note: Currently this function only has code for handling the first handshake packet
 *     Implementation of the third handshake ACK to handle the code
 */
int tcp_nip_rcv_state_process(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	const struct tcphdr *th = tcp_hdr(skb);
	int queued = 0;
	bool acceptable;

	/* Step 1: Connect handshake packet processing */
	switch (sk->sk_state) {
	case TCP_CLOSE:
		goto discard;

	case TCP_LISTEN:
		if (th->ack)
			return 1;

		if (th->rst)
			goto discard;

		if (th->syn) {
			if (th->fin)
				goto discard;

			rcu_read_lock();
			local_bh_disable();
			acceptable = icsk->icsk_af_ops->conn_request(sk, skb) >= 0;
			local_bh_enable();
			rcu_read_unlock();

			if (!acceptable)
				return 1;
			consume_skb(skb);
			return 0;
		}
		goto discard;
	case TCP_SYN_SENT:
		nip_dbg("%s TCP_SYN_SENT", __func__);
		tp->rx_opt.saw_tstamp = 0;
		tcp_mstamp_refresh(tp);
		queued = tcp_nip_rcv_synsent_state_process(sk, skb, th);
		if (queued >= 0)
			return queued;
		__kfree_skb(skb);
		return 0;
	}
	tcp_mstamp_refresh(tp);
	tp->rx_opt.saw_tstamp = 0;

	if (!th->ack && !th->rst && !th->syn)
		goto discard;

	if (!tcp_nip_validate_incoming(sk, skb, th, 0))
		return 0;

	acceptable = tcp_nip_ack(sk, skb);
	/* If the third handshake ACK is invalid, 1 is returned
	 * and the SKB is discarded in tcp_nip_rcv
	 */
	if (!acceptable) {
		if (sk->sk_state == TCP_SYN_RECV)
			return 1;
		goto discard;
	}

	switch (sk->sk_state) {
	case TCP_SYN_RECV:
		tp->copied_seq = tp->rcv_nxt;
		tcp_nip_init_buffer_space(sk);
		/* Invoke memory barrier (annotated prior to checkpatch requirements) */
		smp_mb();
		tcp_set_state(sk, TCP_ESTABLISHED);
		nip_dbg("TCP_ESTABLISHED");
		sk->sk_state_change(sk);

		/* Sets the part to be sent, and the size of the send window */
		tp->snd_una = TCP_SKB_CB(skb)->ack_seq;
		tp->snd_wnd = ntohs(th->window) << tp->rx_opt.snd_wscale;
		tcp_init_wl(tp, TCP_SKB_CB(skb)->seq);

		tp->lsndtime = tcp_jiffies32;

		tcp_initialize_rcv_mss(sk);
		break;
	case TCP_FIN_WAIT1: {
		if (tp->snd_una != tp->write_seq) {
			nip_dbg("%s: tp->snd_una != tp->write_seq", __func__);
			break;
		}

		tcp_set_state(sk, TCP_FIN_WAIT2);
		sk->sk_shutdown |= SEND_SHUTDOWN;

		if (TCP_SKB_CB(skb)->end_seq != TCP_SKB_CB(skb)->seq &&
		    after(TCP_SKB_CB(skb)->end_seq - th->fin, tp->rcv_nxt)) {
			tcp_nip_done(sk);
			nip_dbg("%s: received payload packets, call tcp_nip_done", __func__);
			return 1;
		}

		nip_dbg("%s: TCP_FIN_WAIT1: recvd ack for fin.Wait for fin from other side",
			__func__);
		inet_csk_reset_keepalive_timer(sk, TCP_NIP_CSK_KEEPALIVE_CYCLE * HZ);

		break;
	}

	case TCP_CLOSING:
		if (tp->snd_una == tp->write_seq) {
			nip_dbg("%s: TCP_CLOSING: recvd ack for fin.Ready to destroy", __func__);
			inet_csk_reset_keepalive_timer(sk, TCP_TIMEWAIT_LEN);
			goto discard;
		}
		break;
	case TCP_LAST_ACK:
		nip_dbg("tcp_nip_rcv_state_process_2: TCP_LAST_ACK");
		if (tp->snd_una == tp->write_seq) {
			nip_dbg("%s: LAST_ACK: recvd ack for fin.Directly destroy", __func__);
			tcp_nip_done(sk);
			goto discard;
		}
		break;
	}

	switch (sk->sk_state) {
	case TCP_CLOSE_WAIT:
		nip_dbg("%s: into TCP_CLOSE_WAIT, rst = %d, seq = %u, end_seq = %u, rcv_nxt = %u",
			__func__, th->rst, TCP_SKB_CB(skb)->seq,
			TCP_SKB_CB(skb)->seq, tp->rcv_nxt);
		fallthrough;
	case TCP_CLOSING:
	case TCP_LAST_ACK:
		if (!before(TCP_SKB_CB(skb)->seq, tp->rcv_nxt)) {
			nip_dbg("%s: break in TCP_LAST_ACK", __func__);
			break;
		}
		nip_dbg("tcp_nip_rcv_state_process_3: TCP_LAST_ACK_2");
		fallthrough;
	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
		/* Reset is required according to RFC 1122.
		 * Do not enter the reset process temporarily
		 */
		if (sk->sk_shutdown & RCV_SHUTDOWN) {
			if (TCP_SKB_CB(skb)->end_seq != TCP_SKB_CB(skb)->seq &&
			    after(TCP_SKB_CB(skb)->end_seq - th->fin, tp->rcv_nxt)) {
				tcp_nip_reset(sk);
				nip_dbg("%s: call tcp_nip_reset", __func__);
				return 1;
			}
		}
		fallthrough;
	case TCP_ESTABLISHED:
		tcp_nip_data_queue(sk, skb);
		queued = 1;
		break;
	}

	if (sk->sk_state != TCP_CLOSE) {
		tcp_nip_data_snd_check(sk);
		tcp_nip_ack_snd_check(sk);
	}

	if (!queued) {
discard:
		tcp_nip_drop(sk, skb);
	}
	return 0;
}

/* Function
 *    Initialize RCV_MSS
 * Parameter
 *    sk: transmission control block
 */
void tcp_nip_initialize_rcv_mss(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	unsigned int hint = min_t(unsigned int, tp->advmss, tp->mss_cache);

	hint = min(hint, tp->rcv_wnd / TCP_NUM_2);
	hint = min(hint, TCP_MSS_DEFAULT);
	hint = max(hint, TCP_MIN_MSS);

	inet_csk(sk)->icsk_ack.rcv_mss = hint;
}

/* Function
 *    Handle the third handshake ACK and return the new control block successfully.
 *    Is the core process for handling ACKS.
 *    (1)Create a child control block. Note that the state of the child control
 *    block is TCP_SYN_RECV
 *    This is different from the TCP_NEW_SYN_RECV control block created when syn was received.
 *    (2)Remove the request control block from the incomplete connection queue
 *    and add it to the completed connection queue
 * Parameter
 *    sk: transmission control block
 *    skb: Transfer control block buffer
 *    req: Request connection control block
 */
struct sock *tcp_nip_check_req(struct sock *sk, struct sk_buff *skb,
			       struct request_sock *req)
{
	struct tcp_options_received tmp_opt;
	struct sock *child;
	const struct tcphdr *th = tcp_hdr(skb);
	__be32 flg = tcp_flag_word(th) & (TCP_FLAG_RST | TCP_FLAG_SYN | TCP_FLAG_ACK);
	bool own_req;

	tmp_opt.saw_tstamp = 0;
	/* Check whether the TCP option exists */
	if (th->doff > (sizeof(struct tcphdr) >> TCP_NIP_4BYTE_PAYLOAD))
		/* Parsing TCP options */
		tcp_nip_parse_options(skb, &tmp_opt, 0, NULL);

	/* ACK but the serial number does not match,
	 * return to the original control block, no processing outside
	 */
	if ((flg & TCP_FLAG_ACK) &&
	    (TCP_SKB_CB(skb)->ack_seq !=
	     tcp_rsk(req)->snt_isn + 1)) {
		nip_dbg("%s ack_seq is wrong", __func__);
		return sk;
	}

	/* The above process guarantees that there is an ACK, if not, return directly */
	if (!(flg & TCP_FLAG_ACK)) {
		nip_dbg("%s No TCP_FLAG_ACK", __func__);
		return NULL;
	}

	/* The ack is valid and the child control block is created.
	 * Note that the state of the child control block is TCP_SYN_RECV
	 */
	child = inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, NULL,
							 req, &own_req);
	if (!child) {
		nip_dbg("%s No listen_overflow", __func__);
		goto listen_overflow;
	}
	nip_dbg("%s creat child sock successfully", __func__);

	sock_rps_save_rxhash(child, skb);
	/* Calculate the time spent synack-ack in three handshakes */
	tcp_synack_rtt_meas(child, req);
	/* Delete the original control block from the incomplete queue
	 * and add it to the completed queue
	 */
	return inet_csk_complete_hashdance(sk, child, req, own_req);

listen_overflow:
	if (!sock_net(sk)->ipv4.sysctl_tcp_abort_on_overflow) {
		inet_rsk(req)->acked = 1;
		return NULL;
	}
	return NULL;
}

