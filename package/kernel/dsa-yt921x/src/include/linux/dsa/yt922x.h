/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _LINUX_DSA_YT922X_H
#define _LINUX_DSA_YT922X_H

#include <linux/bitfield.h>
#include <linux/types.h>

/* YT9224 four-byte port tag, following the configurable two-byte TPID.
 * TX: one destination bit per physical port, starting at bit 5.
 * RX: source physical port in bits 7:4. Customer VLAN tags follow this tag.
 * Do not reuse the eight-byte format's force-destination or priority bits.
 */
#define YT922X_4B_TX_PORT_M	GENMASK(13, 5)
#define YT922X_4B_RX_PORT_M	GENMASK(7, 4)

static inline u16 yt922x_4b_port_tag(unsigned int port)
{
	if (port > 8)
		return 0;

	return FIELD_PREP(YT922X_4B_TX_PORT_M, BIT(port));
}

#endif
