// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Motorcomm YT921x Switch Extended CPU Port Tagging
 * Motorcomm YT922x Switch Extended CPU Port Tagging
 *
 * Copyright (c) 2025 David Yang <mmyangfl@gmail.com>
 * Copyright (c) 2026 Kyle switch <kyle.switch@motor-comm.com>
 *
 * +----+----+-------+-----+----+---------
 * | DA | SA | TagET | Tag | ET | Payload ...
 * +----+----+-------+-----+----+---------
 *   6    6      2      6    2       N
 *
 * Tag Ethertype: CPU_TAG_TPID_TPID (default: ETH_P_YT921X = 0x9988)
 *   * Hardcoded for the moment, but still configurable. Discuss it if there
 *     are conflicts somewhere and/or you want to change it for some reason.
 * Tag:
 *   2: VLAN Tag
 *   2:
 *     15b: Rx Port Valid
 *     14b-11b: Rx Port
 *     10b-8b: Tx/Rx Priority
 *     7b: Tx/Rx Code Valid
 *     6b-1b: Tx/Rx Code
 *     0b: ? (unset)
 *   2:
 *     15b: Tx Port(s) Valid
 *     10b-0b: Tx Port(s) Mask
 */

#include <linux/etherdevice.h>

#include "tag.h"
#include "yt921x_proto.h"

#define YT921X_TAG_NAME	"yt921x"

#define YT921X_TAG_LEN	8

#define YT921X_TAG_PORT_EN		BIT(15)
#define YT921X_TAG_RX_PORT_M		GENMASK(14, 11)
#define YT921X_TAG_PRIO_M		GENMASK(10, 8)
#define  YT921X_TAG_PRIO(x)			FIELD_PREP(YT921X_TAG_PRIO_M, (x))
#define YT921X_TAG_CODE_EN		BIT(7)
#define YT921X_TAG_CODE_M		GENMASK(6, 1)
#define  YT921X_TAG_CODE(x)			FIELD_PREP(YT921X_TAG_CODE_M, (x))
#define YT921X_TAG_TX_PORTS_M		GENMASK(10, 0)
#define  YT921X_TAG_TX_PORTS(x)			FIELD_PREP(YT921X_TAG_TX_PORTS_M, (x))

/* Incomplete. Some are configurable via RMA_CTRL_CPU_CODE, the meaning of
 * others remains unknown.
 */
enum yt921x_tag_code {
	YT921X_TAG_CODE_FORWARD = 0,
	YT921X_TAG_CODE_ACL = 0x17,
	YT921X_TAG_CODE_UNK_UCAST = 0x19,
	YT921X_TAG_CODE_UNK_MCAST = 0x1a,
	YT921X_TAG_CODE_PORT_COPY = 0x1b,
	YT921X_TAG_CODE_FDB_COPY = 0x1c,
};

static struct sk_buff *
yt921x_tag_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	__be16 *tag;
	u16 ctrl;

	skb_push(skb, YT921X_TAG_LEN);
	dsa_alloc_etype_header(skb, YT921X_TAG_LEN);

	tag = dsa_etype_header_pos_tx(skb);

	tag[0] = htons(ETH_P_YT921X);
	/* VLAN tag unrelated when TX */
	tag[1] = 0;
	ctrl = YT921X_TAG_CODE(YT921X_TAG_CODE_FORWARD) | YT921X_TAG_CODE_EN |
	       YT921X_TAG_PRIO(skb->priority);
	tag[2] = htons(ctrl);
	ctrl = YT921X_TAG_TX_PORTS(dsa_xmit_port_mask(skb, netdev)) |
	       YT921X_TAG_PORT_EN;
	tag[3] = htons(ctrl);

	return skb;
}

static struct sk_buff *
yt921x_tag_rcv(struct sk_buff *skb, struct net_device *netdev)
{
	unsigned int port;
	__be16 *tag;
	u16 rx;

	if (unlikely(!pskb_may_pull(skb, YT921X_TAG_LEN))) {
		kfree_skb(skb);
		return NULL;
	}

	tag = dsa_etype_header_pos_rx(skb);

	if (unlikely(tag[0] != htons(ETH_P_YT921X))) {
		dev_warn_ratelimited(&netdev->dev,
				     "Unexpected EtherType 0x%04x\n",
				     ntohs(tag[0]));
		kfree_skb(skb);
		return NULL;
	}

	/* Locate which port this is coming from */
	rx = ntohs(tag[2]);
	if (unlikely((rx & YT921X_TAG_PORT_EN) == 0)) {
		dev_warn_ratelimited(&netdev->dev,
				     "Unexpected rx tag 0x%04x\n", rx);
		kfree_skb(skb);
		return NULL;
	}

	port = FIELD_GET(YT921X_TAG_RX_PORT_M, rx);
	skb->dev = dsa_conduit_find_user(netdev, 0, port);
	if (unlikely(!skb->dev)) {
		dev_warn_ratelimited(&netdev->dev,
				     "Couldn't decode source port %u\n", port);
		kfree_skb(skb);
		return NULL;
	}

	skb->priority = FIELD_GET(YT921X_TAG_PRIO_M, rx);

	if (!(rx & YT921X_TAG_CODE_EN)) {
		dev_warn_ratelimited(&netdev->dev,
				     "Tag code not enabled in rx packet\n");
	} else {
		u16 code = FIELD_GET(YT921X_TAG_CODE_M, rx);

		switch (code) {
		case YT921X_TAG_CODE_FORWARD:
		case YT921X_TAG_CODE_PORT_COPY:
		case YT921X_TAG_CODE_FDB_COPY:
			/* Already forwarded by hardware */
			dsa_default_offload_fwd_mark(skb);
			break;
		case YT921X_TAG_CODE_ACL:
		case YT921X_TAG_CODE_UNK_UCAST:
		case YT921X_TAG_CODE_UNK_MCAST:
			/* NOTE: hardware doesn't distinguish between TRAP (copy
			 * to CPU only) and COPY (forward and copy to CPU). In
			 * order to perform a soft switch, NEVER use COPY action
			 * in the switch driver.
			 */
			break;
		default:
			dev_warn_ratelimited(&netdev->dev,
					     "Unknown code 0x%02x\n", code);
			break;
		}
	}

	/* Remove YT921x tag and update checksum */
	skb_pull_rcsum(skb, YT921X_TAG_LEN);
	dsa_strip_etype_header(skb, YT921X_TAG_LEN);

	return skb;
}

static const struct dsa_device_ops yt921x_netdev_ops = {
	.name	= YT921X_TAG_NAME,
	.proto	= DSA_TAG_PROTO_YT921X,
	.xmit	= yt921x_tag_xmit,
	.rcv	= yt921x_tag_rcv,
	.needed_headroom = YT921X_TAG_LEN,
};

DSA_TAG_DRIVER(yt921x_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_YT921X, YT921X_TAG_NAME);

/* To define the from cpu tag format 8 bytes:
 *
 * 0 1 2 3 4 5 6 7 |0 1 2 3 4 5 6 7
 *|<------------TPID 0x9988------->|
 *|<--RESERVE-->|<-----DST POR---->|
 *|-|<---------RESERVE------------>|
 *|<------------------------------>|
 */
#define YT922X_TAG_NAME			"yt922x"
#define YT922X_TAG_PORTMASK_0		BIT(15)
#define YT922X_TAG_PORTMASK_M		GENMASK(8, 0)
#define YT922X_TAG_PORTS(x)		FIELD_PREP(YT922X_TAG_PORTMASK_M, (x))
#define YT922X_TAG_FORCE_DST		BIT(9)
#define YT922X_TAG_PRIO_M		GENMASK(12, 10)
#define YT922X_TAG_PRIO_EN		BIT(13)
#define YT922X_TAG_PRIO(x)		(FIELD_PREP(YT922X_TAG_PRIO_M, (x)) | YT922X_TAG_PRIO_EN)
#define YT922X_TAG_RX_PORT_M		GENMASK(5, 2)
#define YT922X_TAG_RX_PRIO_M		GENMASK(15, 13)

static struct sk_buff *
yt922x_tag_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct dsa_port *dp = dsa_user_to_port(netdev);
	__be16 *tag;
	u16 ctrl;

	skb_push(skb, YT921X_TAG_LEN);
	dsa_alloc_etype_header(skb, YT921X_TAG_LEN);
	tag = dsa_etype_header_pos_tx(skb);

	tag[0] = htons(ETH_P_YT921X);
	if (dp->index != 0) {
		/* Port index is not equal 0 in tag[1] */
		ctrl = YT922X_TAG_PRIO(skb->priority) | YT922X_TAG_FORCE_DST |
			YT922X_TAG_PORTS(dsa_xmit_port_mask(skb, netdev) - 1);
		tag[1] = htons(ctrl);
		tag[2] = 0;
	} else {
		/* Port 0 in bit15 in tag[2] */
		ctrl = YT922X_TAG_PRIO(skb->priority) | YT922X_TAG_FORCE_DST;
		tag[1] = htons(ctrl);
		ctrl = YT922X_TAG_PORTMASK_0;
		tag[2] = htons(ctrl);
	}
	tag[3] = 0;

	return skb;
}

static struct sk_buff *
yt922x_tag_rcv(struct sk_buff *skb, struct net_device *netdev)
{
	unsigned int port;
	__be16 *tag;
	u16 rx;

	if (unlikely(!pskb_may_pull(skb, YT921X_TAG_LEN))) {
		kfree_skb(skb);
		return NULL;
	}

	tag = dsa_etype_header_pos_rx(skb);

	if (unlikely(tag[0] != htons(ETH_P_YT921X))) {
		dev_warn_ratelimited(&netdev->dev,
				     "Unexpected EtherType 0x%04x\n",
				     ntohs(tag[0]));
		kfree_skb(skb);
		return NULL;
	}

	/* Locate which port this is coming from */
	rx = ntohs(tag[2]);
	port = FIELD_GET(YT922X_TAG_RX_PORT_M, rx);
	skb->dev = dsa_conduit_find_user(netdev, 0, port);
	if (unlikely(!skb->dev)) {
		dev_warn_ratelimited(&netdev->dev,
				     "Couldn't decode source port %u\n", port);
		kfree_skb(skb);
		return NULL;
	}

	/* Remove tag and update checksum */
	skb_pull_rcsum(skb, YT921X_TAG_LEN);
	dsa_strip_etype_header(skb, YT921X_TAG_LEN);

	return skb;
}

static const struct dsa_device_ops yt922x_netdev_ops = {
	.name   = YT922X_TAG_NAME,
	.proto  = DSA_TAG_PROTO_YT922X,
	.xmit   = yt922x_tag_xmit,
	.rcv    = yt922x_tag_rcv,
	.needed_headroom = YT921X_TAG_LEN,
};

DSA_TAG_DRIVER(yt922x_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_YT922X, YT922X_TAG_NAME);

static struct dsa_tag_driver *dsa_tag_driver_array[] = {
	&DSA_TAG_DRIVER_NAME(yt921x_netdev_ops),
	&DSA_TAG_DRIVER_NAME(yt922x_netdev_ops),
};
module_dsa_tag_drivers(dsa_tag_driver_array);

MODULE_DESCRIPTION("DSA tag driver for Motorcomm YT921x and YT922x switches");
MODULE_LICENSE("GPL");

