/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Out-of-tree glue for the Motorcomm YT921x/YT922x DSA driver on 6.12.
 *
 * Upstream adds these to <linux/if_ether.h> and <net/dsa.h>. Built out of
 * tree they live here instead, so the kernel headers - and the kernel
 * config the vermagic is derived from - stay exactly as the official build
 * has them. The DSA core only ever looks the tagger up by this number, so
 * any value no in-tree tagger uses will do; 32/33 match what the driver
 * uses on OpenWrt main, where MXL862 already holds 30 and 31.
 */

#ifndef __YT921X_PROTO_H
#define __YT921X_PROTO_H

#include <net/dsa.h>

#ifndef ETH_P_YT921X
#define ETH_P_YT921X			0x9988
#endif

#define DSA_TAG_PROTO_YT921X_VALUE	32
#define DSA_TAG_PROTO_YT922X_VALUE	33

#define DSA_TAG_PROTO_YT921X		((enum dsa_tag_protocol)DSA_TAG_PROTO_YT921X_VALUE)
#define DSA_TAG_PROTO_YT922X		((enum dsa_tag_protocol)DSA_TAG_PROTO_YT922X_VALUE)

#endif /* __YT921X_PROTO_H */
