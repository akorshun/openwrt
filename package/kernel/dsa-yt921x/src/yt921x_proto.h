/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Out-of-tree glue for the Motorcomm YT921x/YT922x DSA driver on 6.12.
 *
 * Upstream adds the tag protocols to enum dsa_tag_protocol in <net/dsa.h>
 * and ETH_P_YT921X to <linux/if_ether.h>. Built out of tree they cannot go
 * there without changing the kernel the official kmods were built against.
 * The protocol numbers come from <linux/dsa/yt922x.h>, which the kernel
 * patch 746-net-ethernet-mtk_ppe-offload-flows-to-YT922x-four-byte-port-tags
 * adds so that the PPE offload code recognises the same four-byte tag: the
 * DSA core only ever looks a tagger up by this number, and 32/33/34 are the
 * values OpenWrt main has in the enum, where MXL862 already holds 30 and 31.
 */

#ifndef __YT921X_PROTO_H
#define __YT921X_PROTO_H

#include <linux/dsa/yt922x.h>
#include <net/dsa.h>

#ifndef ETH_P_YT921X
#define ETH_P_YT921X			0x9988
#endif

#endif /* __YT921X_PROTO_H */
