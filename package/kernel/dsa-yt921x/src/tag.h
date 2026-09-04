/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Out-of-tree copy of the bits of net/dsa/tag.h that the YT921x tagger
 * needs. The in-kernel header is private to net/dsa and pulls in port.h
 * and user.h, which are not shipped with the kernel headers; everything
 * below only relies on <net/dsa.h> and symbols the DSA core exports for
 * modular taggers. dsa_xmit_port_mask() is the helper the driver series
 * adds upstream in 6.19 ("net: dsa: introduce the dsa xmit port mask
 * tagging protocol helper"), reproduced here for 6.12.
 */

#ifndef __DSA_TAG_H
#define __DSA_TAG_H

#include <linux/if_vlan.h>
#include <linux/list.h>
#include <linux/types.h>
#include <net/dsa.h>

struct dsa_tag_driver {
	const struct dsa_device_ops *ops;
	struct list_head list;
	struct module *owner;
};

static inline struct net_device *dsa_conduit_find_user(struct net_device *dev,
						       int device, int port)
{
	struct dsa_port *cpu_dp = dev->dsa_ptr;
	struct dsa_switch_tree *dst = cpu_dp->dst;
	struct dsa_port *dp;

	list_for_each_entry(dp, &dst->ports, list)
		if (dp->ds->index == device && dp->index == port &&
		    dp->type == DSA_PORT_TYPE_USER)
			return dp->user;

	return NULL;
}

/* If the ingress port offloads the bridge, we mark the frame as autonomously
 * forwarded by hardware, so the software bridge doesn't forward in twice, back
 * to us, because we already did. However, if we're in fallback mode and we do
 * software bridging, we are not offloading it, therefore the dp->bridge
 * pointer is not populated, and flooding needs to be done by software (we are
 * effectively operating in standalone ports mode).
 */
static inline void dsa_default_offload_fwd_mark(struct sk_buff *skb)
{
	struct dsa_port *dp = dsa_user_to_port(skb->dev);

	skb->offload_fwd_mark = !!(dp->bridge);
}

/* Helper for removing DSA header tags from packets in the RX path.
 * Must not be called before skb_pull(len).
 */
static inline void dsa_strip_etype_header(struct sk_buff *skb, int len)
{
	memmove(skb->data - ETH_HLEN, skb->data - ETH_HLEN - len, 2 * ETH_ALEN);
}

/* Helper for creating space for DSA header tags in TX path packets.
 * Must not be called before skb_push(len).
 */
static inline void dsa_alloc_etype_header(struct sk_buff *skb, int len)
{
	memmove(skb->data, skb->data + len, 2 * ETH_ALEN);
}

/* On RX, eth_type_trans() on the DSA conduit pulls ETH_HLEN bytes starting from
 * skb_mac_header(skb), which leaves skb->data pointing at the first byte after
 * what the DSA conduit perceives as the EtherType (the beginning of the L3
 * protocol). Since DSA EtherType header taggers treat the EtherType as part of
 * the DSA tag itself, and the EtherType is 2 bytes in length, the DSA header
 * is located 2 bytes behind skb->data.
 */
static inline void *dsa_etype_header_pos_rx(struct sk_buff *skb)
{
	return skb->data - 2;
}

/* On TX, skb->data points to the MAC header, which means that EtherType
 * header taggers start exactly where the EtherType is (the EtherType is
 * treated as part of the DSA header).
 */
static inline void *dsa_etype_header_pos_tx(struct sk_buff *skb)
{
	return skb->data + 2 * ETH_ALEN;
}

/* Port mask a frame leaving @dev has to be sent to: the port itself, plus
 * the other ports of its HSR ring when the hardware duplicates HSR frames.
 */
static inline unsigned long dsa_xmit_port_mask(const struct sk_buff *skb,
					       const struct net_device *dev)
{
	struct dsa_port *dp = dsa_user_to_port(dev);
	unsigned long mask = BIT(dp->index);

	if (IS_ENABLED(CONFIG_HSR) &&
	    unlikely(dev->features & NETIF_F_HW_HSR_DUP)) {
		struct net_device *hsr_dev = dp->hsr_dev;
		struct dsa_port *other_dp;

		dsa_hsr_foreach_port(other_dp, dp->ds, hsr_dev)
			mask |= BIT(other_dp->index);
	}

	return mask;
}

/* Create 2 modaliases per tagging protocol, one to auto-load the module
 * given the ID reported by get_tag_protocol(), and the other by name.
 */
#define DSA_TAG_DRIVER_ALIAS "dsa_tag:"
#define MODULE_ALIAS_DSA_TAG_DRIVER(__proto, __name) \
	MODULE_ALIAS(DSA_TAG_DRIVER_ALIAS __name); \
	MODULE_ALIAS(DSA_TAG_DRIVER_ALIAS "id-" \
		     __stringify(__proto##_VALUE))

void dsa_tag_drivers_register(struct dsa_tag_driver *dsa_tag_driver_array[],
			      unsigned int count,
			      struct module *owner);
void dsa_tag_drivers_unregister(struct dsa_tag_driver *dsa_tag_driver_array[],
				unsigned int count);

#define dsa_tag_driver_module_drivers(__dsa_tag_drivers_array, __count)	\
static int __init dsa_tag_driver_module_init(void)			\
{									\
	dsa_tag_drivers_register(__dsa_tag_drivers_array, __count,	\
				 THIS_MODULE);				\
	return 0;							\
}									\
module_init(dsa_tag_driver_module_init);				\
									\
static void __exit dsa_tag_driver_module_exit(void)			\
{									\
	dsa_tag_drivers_unregister(__dsa_tag_drivers_array, __count);	\
}									\
module_exit(dsa_tag_driver_module_exit)

#define module_dsa_tag_drivers(__ops_array)				\
dsa_tag_driver_module_drivers(__ops_array, ARRAY_SIZE(__ops_array))

#define DSA_TAG_DRIVER_NAME(__ops) dsa_tag_driver ## _ ## __ops

#define DSA_TAG_DRIVER(__ops)						\
static struct dsa_tag_driver DSA_TAG_DRIVER_NAME(__ops) = {		\
	.ops = &__ops,							\
}

#endif
