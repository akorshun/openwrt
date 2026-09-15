// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Motorcomm YT922x Switch Extended CPU Port Tagging
 *
 * Copyright (c) 2026 Kyle switch <kyle.switch@motor-comm.com>
 *
 */

#include <linux/etherdevice.h>
#include <linux/dsa/yt922x.h>

#include "tag.h"
#include "yt921x_proto.h"

#define YT922X_TAG_LEN	8

/*
 * To define the from cpu tag format 8 bytes:
 */
#define YT922X_TAG_NAME			"yt922x"
#define YT922X_TAG_PORTMASK_0		BIT(15)
#define YT922X_TAG_PORTMASK_M		GENMASK(8, 0)
#define  YT922X_TAG_PORTS(x)			FIELD_PREP(YT922X_TAG_PORTMASK_M, ((x) >> 0x1))
#define YT922X_TAG_FORCE_DST		BIT(9)
#define YT922X_TAG_PRIO_M		GENMASK(12, 10)
#define YT922X_TAG_PRIO_EN		BIT(13)
#define  YT922X_TAG_PRIO(x)			(FIELD_PREP(YT922X_TAG_PRIO_M, (x)) | YT922X_TAG_PRIO_EN)
#define YT922X_TAG_RX_PORT_M		GENMASK(5, 2)
#define YT922X_TAG_RX_PRIO_M		GENMASK(15, 13)

static struct sk_buff *
yt922x_tag_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct dsa_port *dp = dsa_user_to_port(netdev);
	__be16 *tag;
	u16 ctrl;

	skb_push(skb, YT922X_TAG_LEN);
	dsa_alloc_etype_header(skb, YT922X_TAG_LEN);
	tag = dsa_etype_header_pos_tx(skb);

	tag[0] = htons(ETH_P_YT921X);
	if (dp->index != 0) {
		/* Port index is not equal 0 in tag[1] */
		ctrl = YT922X_TAG_PRIO(skb->priority) | YT922X_TAG_FORCE_DST |
			YT922X_TAG_PORTS(BIT(dp->index));
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

	if (unlikely(!pskb_may_pull(skb, YT922X_TAG_LEN))) {
		return NULL;
	}

	tag = dsa_etype_header_pos_rx(skb);

	if (unlikely(tag[0] != htons(ETH_P_YT921X))) {
		dev_warn_ratelimited(&netdev->dev,
				     "Unexpected EtherType 0x%04x\n",
				     ntohs(tag[0]));
		return NULL;
	}

	/* Locate which port this is coming from */
	rx = ntohs(tag[2]);
	port = FIELD_GET(YT922X_TAG_RX_PORT_M, rx);
	skb->dev = dsa_conduit_find_user(netdev, 0, port);
	if (unlikely(!skb->dev)) {
		dev_warn_ratelimited(&netdev->dev,
				     "Couldn't decode source port %u\n", port);
		return NULL;
	}

	/* Remove tag and update checksum */
	skb_pull_rcsum(skb, YT922X_TAG_LEN);
	dsa_strip_etype_header(skb, YT922X_TAG_LEN);

	/* Broadcast is flooded in hardware. Unknown unicast and multicast
	 * remain trapped, so they must still be forwarded by the bridge.
	 */
	if (is_broadcast_ether_addr(eth_hdr(skb)->h_dest))
		dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops yt922x_netdev_ops = {
	.name   = YT922X_TAG_NAME,
	.proto  = DSA_TAG_PROTO_YT922X,
	.xmit   = yt922x_tag_xmit,
	.rcv    = yt922x_tag_rcv,
	.needed_headroom = YT922X_TAG_LEN,
};

static struct sk_buff *
yt922x_4b_tag_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct dsa_port *dp = dsa_user_to_port(netdev);
	u16 ctrl = yt922x_4b_port_tag(dp->index);
	__be16 *tag;

	if (unlikely(!ctrl))
		return NULL;

	skb_push(skb, VLAN_HLEN);
	dsa_alloc_etype_header(skb, VLAN_HLEN);
	tag = dsa_etype_header_pos_tx(skb);
	tag[0] = htons(ETH_P_8021Q);
	tag[1] = htons(ctrl);

	return skb;
}

static struct sk_buff *
yt922x_4b_tag_rcv(struct sk_buff *skb, struct net_device *netdev)
{
	struct net_device *user;
	unsigned int port;
	u16 ctrl;

	if (skb_vlan_tag_present(skb)) {
		if (unlikely(skb->vlan_proto != htons(ETH_P_8021Q)))
			return NULL;
		ctrl = skb_vlan_tag_get(skb);
		__vlan_hwaccel_clear_tag(skb);
	} else {
		__be16 *tag;

		if (unlikely(!pskb_may_pull(skb, VLAN_HLEN)))
			return NULL;
		tag = dsa_etype_header_pos_rx(skb);
		if (unlikely(tag[0] != htons(ETH_P_8021Q)))
			return NULL;
		ctrl = ntohs(tag[1]);
		skb_pull_rcsum(skb, VLAN_HLEN);
		dsa_strip_etype_header(skb, VLAN_HLEN);
	}

	port = FIELD_GET(YT922X_4B_RX_PORT_M, ctrl);
	user = dsa_conduit_find_user(netdev, 0, port);
	if (unlikely(!user))
		return NULL;
	skb->dev = user;

	if (is_broadcast_ether_addr(eth_hdr(skb)->h_dest))
		dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops yt922x_4b_netdev_ops = {
	.name = "yt922x_4b",
	.proto = DSA_TAG_PROTO_YT922X_4B,
	.xmit = yt922x_4b_tag_xmit,
	.rcv = yt922x_4b_tag_rcv,
	.needed_headroom = VLAN_HLEN,
};

DSA_TAG_DRIVER(yt922x_netdev_ops);
DSA_TAG_DRIVER(yt922x_4b_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_YT922X, YT922X_TAG_NAME);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_YT922X_4B, "yt922x_4b");

static struct dsa_tag_driver *yt922x_tag_drivers[] = {
	&DSA_TAG_DRIVER_NAME(yt922x_netdev_ops),
	&DSA_TAG_DRIVER_NAME(yt922x_4b_netdev_ops),
};
module_dsa_tag_drivers(yt922x_tag_drivers);

MODULE_DESCRIPTION("DSA tag driver for Motorcomm YT922x switches");
MODULE_LICENSE("GPL");
