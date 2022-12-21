/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP INET
 * An implementation of the TCP/IP protocol suite for the LINUX
 * operating system. NewIP INET is implemented using the  BSD Socket
 * interface as the means of communication with the user level.
 *
 * Definitions for the NewIP parameter module.
 */
#ifndef _TCP_NIP_PARAMETER_H
#define _TCP_NIP_PARAMETER_H

/*********************************************************************************************/
/*                            Rto timeout timer period (HZ/n)                                */
/*********************************************************************************************/
extern int g_nip_rto;

/*********************************************************************************************/
/*                            TCP sending and receiving buffer configuration                 */
/*********************************************************************************************/
extern int g_nip_sndbuf;
extern int g_nip_rcvbuf;

/*********************************************************************************************/
/*                            Window configuration                                           */
/*********************************************************************************************/
extern int g_wscale_enable;
extern int g_wscale;

/*********************************************************************************************/
/*                            Enables the debugging of special scenarios                     */
/*********************************************************************************************/
extern int g_ack_num;
extern int g_nip_ssthresh_reset;

/*********************************************************************************************/
/*                            Retransmission parameters after ACK                            */
/*********************************************************************************************/
extern int g_dup_ack_retrans_num;
extern int g_ack_retrans_num;
extern int g_dup_ack_snd_max;

/*********************************************************************************************/
/*                            RTT timestamp parameters                                       */
/*********************************************************************************************/
extern int g_rtt_tstamp_rto_up;
extern int g_rtt_tstamp_high;
extern int g_rtt_tstamp_mid_high;
extern int g_rtt_tstamp_mid_low;
extern int g_ack_to_nxt_snd_tstamp;

/*********************************************************************************************/
/*                            Window threshold parameters                                    */
/*********************************************************************************************/
extern int g_ssthresh_enable;
extern int g_nip_ssthresh_default;
extern int g_ssthresh_high;
extern int g_ssthresh_mid_high;
extern int g_ssthresh_mid_low;
extern int g_ssthresh_low;
extern int g_ssthresh_low_min;
extern int g_ssthresh_high_step;

/*********************************************************************************************/
/*                            keepalive parameters                                           */
/*********************************************************************************************/
extern int g_nip_idle_ka_probes_out;
extern int g_nip_keepalive_time;
extern int g_nip_keepalive_intvl;

/*********************************************************************************************/
/*                            window mode parameters                                         */
/*********************************************************************************************/
extern bool g_nip_tcp_snd_win_enable;
extern bool g_nip_tcp_rcv_win_enable;

/*********************************************************************************************/
/*                            probe parameters                                               */
/*********************************************************************************************/
extern int g_nip_probe_max;

/*********************************************************************************************/
/*                            nip debug parameters                                           */
/*********************************************************************************************/
/* Debugging for control DEBUG */
extern bool g_nip_debug;

/* Debugging of threshold change */
extern int g_rtt_ssthresh_debug;
#define ssthresh_dbg(fmt, ...) \
do { \
	if (g_rtt_ssthresh_debug) \
		pr_crit(fmt, ##__VA_ARGS__); \
} while (0)

/* Debugging of packet retransmission after ACK */
extern int g_ack_retrans_debug;
#define retrans_dbg(fmt, ...) \
do { \
	if (g_ack_retrans_debug) \
		pr_crit(fmt, ##__VA_ARGS__); \
} while (0)

#endif	/* _TCP_NIP_PARAMETER_H */
