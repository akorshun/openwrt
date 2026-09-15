// SPDX-License-Identifier: GPL-2.0+
/*
 * Motorcomm 8511/8521/8522/8531/8531S/8821/8824 PHY driver.
 *
 * Author: Peter Geis <pgwipeout@gmail.com>
 * Author: Frank <Frank.Sae@motor-comm.com>
 * Author: Kyle <kyle.switch@motor-comm.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/property.h>

#include <linux/mdio.h>

/*
 * Built out of tree for OpenWrt 25.12 (Linux 6.12), so the kernel config and
 * with it the vermagic of the official kmods stay untouched. What 6.12 lacks
 * compared with the tree this driver was written for is carried here.
 */

/* drivers/net/phy/phylib.h got these accessors in 6.16; on 6.12 the shared
 * structure is still public in <linux/phy.h>.
 */
static inline void *phy_package_get_priv(struct phy_device *phydev)
{
	return phydev->shared->priv;
}

static inline struct device_node *phy_package_get_node(struct phy_device *phydev)
{
	return phydev->shared->np;
}

/* From the same YT8824 series: 10GBASE-T template test mode register, added
 * to phy-c45.c there. Carried locally instead of patching the built-in phylib.
 */
#ifndef MDIO_PMA_10GBT_TESTMODE
#define MDIO_PMA_10GBT_TESTMODE			132
#define MDIO_PMA_10GBT_TESTMODE_TEMPLATE	0xE000
#define MDIO_PMA_10GBT_TESTMODE_NORMAL		0x0000
#define MDIO_PMA_10GBT_TESTMODE_1		0x2000
#define MDIO_PMA_10GBT_TESTMODE_2		0x4000
#define MDIO_PMA_10GBT_TESTMODE_3		0x6000
#define MDIO_PMA_10GBT_TESTMODE_4		0x8000
#define MDIO_PMA_10GBT_TESTMODE_5		0xa000
#define MDIO_PMA_10GBT_TESTMODE_6		0xc000
#define MDIO_PMA_10GBT_TESTMODE_7		0xe000
#endif

static int genphy_c45_template_testmode(struct phy_device *phydev, int test_mode)
{
	static const u16 ctrl[] = {
		MDIO_PMA_10GBT_TESTMODE_NORMAL, MDIO_PMA_10GBT_TESTMODE_1,
		MDIO_PMA_10GBT_TESTMODE_2, MDIO_PMA_10GBT_TESTMODE_3,
		MDIO_PMA_10GBT_TESTMODE_4, MDIO_PMA_10GBT_TESTMODE_5,
		MDIO_PMA_10GBT_TESTMODE_6, MDIO_PMA_10GBT_TESTMODE_7,
	};

	if (test_mode < 0 || test_mode >= ARRAY_SIZE(ctrl))
		return -EINVAL;

	return phy_modify_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBT_TESTMODE,
			      MDIO_PMA_10GBT_TESTMODE_TEMPLATE, ctrl[test_mode]);
}

#define PHY_ID_YT8511		0x0000010a
#define PHY_ID_YT8521		0x0000011a
#define PHY_ID_YT8522		0x4f51e928
#define PHY_ID_YT8531		0x4f51e91b
#define PHY_ID_YT8531S		0x4f51e91a
#define PHY_ID_YT8821		0x4f51ea19
#define PHY_ID_YT8824		0x4f51e8b8
/* YT8521/YT8531S/YT8821 Register Overview
 *	UTP Register space	|	FIBER Register space
 *  ------------------------------------------------------------
 * |	UTP MII			|	FIBER MII		|
 * |	UTP MMD			|				|
 * |	UTP Extended		|	FIBER Extended		|
 *  ------------------------------------------------------------
 * |			Common Extended				|
 *  ------------------------------------------------------------
 */

/* YT8824 Register Overview
 *	UTP Register space	|	FIBER Register space
 *  ------------------------------------------------------------
 * |	UTP MII			|	FIBER MII	        |
 * |	UTP MMD			|				|
 * |	UTP Extended		|	FIBER Extended		|
 * |	UTP Top Extended	|	FIBER Top Extended	|
 *  ------------------------------------------------------------
 * |			Common Top Extended			|
 *  ------------------------------------------------------------
 */

/* 0x10 ~ 0x15 , 0x1E and 0x1F are common MII registers of yt phy */

/* Specific Function Control Register */
#define YTPHY_SPECIFIC_FUNCTION_CONTROL_REG	0x10

/* 2b00 Manual MDI configuration
 * 2b01 Manual MDIX configuration
 * 2b10 Reserved
 * 2b11 Enable automatic crossover for all modes  *default*
 */
#define YTPHY_SFCR_MDI_CROSSOVER_MODE_MASK	(BIT(6) | BIT(5))
#define YTPHY_SFCR_CROSSOVER_EN			BIT(3)
#define YTPHY_SFCR_SQE_TEST_EN			BIT(2)
#define YTPHY_SFCR_POLARITY_REVERSAL_EN		BIT(1)
#define YTPHY_SFCR_JABBER_DIS			BIT(0)

/* Specific Status Register */
#define YTPHY_SPECIFIC_STATUS_REG		0x11
#define YTPHY_SSR_SPEED_MASK			((0x3 << 14) | BIT(9))
#define YTPHY_SSR_SPEED_10M			((0x0 << 14))
#define YTPHY_SSR_SPEED_100M			((0x1 << 14))
#define YTPHY_SSR_SPEED_1000M			((0x2 << 14))
#define YTPHY_SSR_SPEED_10G			((0x3 << 14))
#define YTPHY_SSR_SPEED_2500M			((0x0 << 14) | BIT(9))
#define YTPHY_SSR_DUPLEX_OFFSET			13
#define YTPHY_SSR_DUPLEX			BIT(13)
#define YTPHY_SSR_PAGE_RECEIVED			BIT(12)
#define YTPHY_SSR_SPEED_DUPLEX_RESOLVED		BIT(11)
#define YTPHY_SSR_LINK				BIT(10)
#define YTPHY_SSR_MDIX_CROSSOVER		BIT(6)
#define YTPHY_SSR_DOWNGRADE			BIT(5)
#define YTPHY_SSR_TRANSMIT_PAUSE		BIT(3)
#define YTPHY_SSR_RECEIVE_PAUSE			BIT(2)
#define YTPHY_SSR_POLARITY			BIT(1)
#define YTPHY_SSR_JABBER			BIT(0)

/* Interrupt enable Register */
#define YTPHY_INTERRUPT_ENABLE_REG		0x12
#define YTPHY_IER_WOL				BIT(6)

/* Interrupt Status Register */
#define YTPHY_INTERRUPT_STATUS_REG		0x13
#define YTPHY_ISR_AUTONEG_ERR			BIT(15)
#define YTPHY_ISR_SPEED_CHANGED			BIT(14)
#define YTPHY_ISR_DUPLEX_CHANGED		BIT(13)
#define YTPHY_ISR_PAGE_RECEIVED			BIT(12)
#define YTPHY_ISR_LINK_FAILED			BIT(11)
#define YTPHY_ISR_LINK_SUCCESSED		BIT(10)
#define YTPHY_ISR_WOL				BIT(6)
#define YTPHY_ISR_WIRESPEED_DOWNGRADE		BIT(5)
#define YTPHY_ISR_SERDES_LINK_FAILED		BIT(3)
#define YTPHY_ISR_SERDES_LINK_SUCCESSED		BIT(2)
#define YTPHY_ISR_POLARITY_CHANGED		BIT(1)
#define YTPHY_ISR_JABBER_HAPPENED		BIT(0)

/* Speed Auto Downgrade Control Register */
#define YTPHY_SPEED_AUTO_DOWNGRADE_CONTROL_REG	0x14
#define YTPHY_SADCR_SPEED_DOWNGRADE_EN		BIT(5)

/* If these bits are set to 3, the PHY attempts five times ( 3(set value) +
 * additional 2) before downgrading, default 0x3
 */
#define YTPHY_SADCR_SPEED_RETRY_LIMIT		(0x3 << 2)

/* Rx Error Counter Register */
#define YTPHY_RX_ERROR_COUNTER_REG		0x15

/* Extended Register's Address Offset Register */
#define YTPHY_PAGE_SELECT			0x1E

/* Extended Register's Data Register */
#define YTPHY_PAGE_DATA				0x1F

/* FIBER Auto-Negotiation link partner ability */
#define YTPHY_FLPA_PAUSE			(0x3 << 7)
#define YTPHY_FLPA_ASYM_PAUSE			(0x2 << 7)

#define YT8511_PAGE_SELECT	0x1e
#define YT8511_PAGE		0x1f
#define YT8511_EXT_CLK_GATE	0x0c
#define YT8511_EXT_DELAY_DRIVE	0x0d
#define YT8511_EXT_SLEEP_CTRL	0x27

/* 2b00 25m from pll
 * 2b01 25m from xtl *default*
 * 2b10 62.m from pll
 * 2b11 125m from pll
 */
#define YT8511_CLK_125M		(BIT(2) | BIT(1))
#define YT8511_PLLON_SLP	BIT(14)

/* RX Delay enabled = 1.8ns 1000T, 8ns 10/100T */
#define YT8511_DELAY_RX		BIT(0)

/* TX Gig-E Delay is bits 7:4, default 0x5
 * TX Fast-E Delay is bits 15:12, default 0xf
 * Delay = 150ps * N - 250ps
 * On = 2000ps, off = 50ps
 */
#define YT8511_DELAY_GE_TX_EN	(0xf << 4)
#define YT8511_DELAY_GE_TX_DIS	(0x2 << 4)
#define YT8511_DELAY_FE_TX_EN	(0xf << 12)
#define YT8511_DELAY_FE_TX_DIS	(0x2 << 12)

/* Extended register is different from MMD Register and MII Register.
 * We can use ytphy_read_ext/ytphy_write_ext/ytphy_modify_ext function to
 * operate extended register.
 * Extended Register  start
 */

/* Phy gmii clock gating Register */
#define YT8521_CLOCK_GATING_REG			0xC
#define YT8521_CGR_RX_CLK_EN			BIT(12)

#define YT8521_EXTREG_SLEEP_CONTROL1_REG	0x27
#define YT8521_ESC1R_SLEEP_SW			BIT(15)
#define YT8521_ESC1R_PLLON_SLP			BIT(14)

/* Phy fiber Link timer cfg2 Register */
#define YT8521_LINK_TIMER_CFG2_REG		0xA5
#define YT8521_LTCR_EN_AUTOSEN			BIT(15)

/* 0xA000, 0xA001, 0xA003, 0xA006 ~ 0xA00A and 0xA012 are common ext registers
 * of yt8521 phy. There is no need to switch reg space when operating these
 * registers.
 */

#define YT8521_REG_SPACE_SELECT_REG		0xA000
#define YT8521_RSSR_SPACE_MASK			BIT(1)
#define YT8521_RSSR_FIBER_SPACE			(0x1 << 1)
#define YT8521_RSSR_UTP_SPACE			(0x0 << 1)
#define YT8521_RSSR_TO_BE_ARBITRATED		(0xFF)

#define YT8521_CHIP_CONFIG_REG			0xA001
#define YT8521_CCR_SW_RST			BIT(15)
#define YT8531_RGMII_LDO_VOL_MASK		GENMASK(5, 4)
#define YT8531_LDO_VOL_3V3			0x0
#define YT8531_LDO_VOL_1V8			0x2

/* 1b0 disable 1.9ns rxc clock delay  *default*
 * 1b1 enable 1.9ns rxc clock delay
 */
#define YT8521_CCR_RXC_DLY_EN			BIT(8)
#define YT8521_CCR_RXC_DLY_1_900_NS		1900

#define YT8521_CCR_MODE_SEL_MASK		(BIT(2) | BIT(1) | BIT(0))
#define YT8521_CCR_MODE_UTP_TO_RGMII		0
#define YT8521_CCR_MODE_FIBER_TO_RGMII		1
#define YT8521_CCR_MODE_UTP_FIBER_TO_RGMII	2
#define YT8521_CCR_MODE_UTP_TO_SGMII		3
#define YT8521_CCR_MODE_SGPHY_TO_RGMAC		4
#define YT8521_CCR_MODE_SGMAC_TO_RGPHY		5
#define YT8521_CCR_MODE_UTP_TO_FIBER_AUTO	6
#define YT8521_CCR_MODE_UTP_TO_FIBER_FORCE	7

/* 3 phy polling modes,poll mode combines utp and fiber mode*/
#define YT8521_MODE_FIBER			0x1
#define YT8521_MODE_UTP				0x2
#define YT8521_MODE_POLL			0x3

#define YT8521_RGMII_CONFIG1_REG		0xA003
/* 1b0 use original tx_clk_rgmii  *default*
 * 1b1 use inverted tx_clk_rgmii.
 */
#define YT8521_RC1R_TX_CLK_SEL_INVERTED		BIT(14)
#define YT8521_RC1R_RX_DELAY_MASK		GENMASK(13, 10)
#define YT8521_RC1R_FE_TX_DELAY_MASK		GENMASK(7, 4)
#define YT8521_RC1R_GE_TX_DELAY_MASK		GENMASK(3, 0)
#define YT8521_RC1R_RGMII_0_000_NS		0
#define YT8521_RC1R_RGMII_0_150_NS		1
#define YT8521_RC1R_RGMII_0_300_NS		2
#define YT8521_RC1R_RGMII_0_450_NS		3
#define YT8521_RC1R_RGMII_0_600_NS		4
#define YT8521_RC1R_RGMII_0_750_NS		5
#define YT8521_RC1R_RGMII_0_900_NS		6
#define YT8521_RC1R_RGMII_1_050_NS		7
#define YT8521_RC1R_RGMII_1_200_NS		8
#define YT8521_RC1R_RGMII_1_350_NS		9
#define YT8521_RC1R_RGMII_1_500_NS		10
#define YT8521_RC1R_RGMII_1_650_NS		11
#define YT8521_RC1R_RGMII_1_800_NS		12
#define YT8521_RC1R_RGMII_1_950_NS		13
#define YT8521_RC1R_RGMII_2_100_NS		14
#define YT8521_RC1R_RGMII_2_250_NS		15

/* LED CONFIG */
#define YT8521_MAX_LEDS				3
#define YT8521_LED0_CFG_REG			0xA00C
#define YT8521_LED1_CFG_REG			0xA00D
#define YT8521_LED2_CFG_REG			0xA00E
#define YT8521_LED_ACT_BLK_IND			BIT(13)
#define YT8521_LED_FDX_ON_EN			BIT(12)
#define YT8521_LED_HDX_ON_EN			BIT(11)
#define YT8521_LED_TXACT_BLK_EN			BIT(10)
#define YT8521_LED_RXACT_BLK_EN			BIT(9)
#define YT8521_LED_1000_ON_EN			BIT(6)
#define YT8521_LED_100_ON_EN			BIT(5)
#define YT8521_LED_10_ON_EN			BIT(4)

#define YTPHY_MDIO_ADDRESS_CONTROL_REG		0xA005
#define YTPHY_MACR_EN_PHY_ADDR_0		BIT(6)

#define YT8522_EXTREG_SLEEP_CONTROL		0x2027
#define YT8522_EN_SLEEP_SW			BIT(15)

#define YT8522_EXTENDED_COMBO_CTRL		0x4000
#define YT8522_RXDV_SEL				BIT(4)
#define YT8522_RMII_EN				BIT(1)

#define YTPHY_MISC_CONFIG_REG			0xA006
#define YTPHY_MCR_FIBER_SPEED_MASK		BIT(0)
#define YTPHY_MCR_FIBER_1000BX			(0x1 << 0)
#define YTPHY_MCR_FIBER_100FX			(0x0 << 0)

/* WOL MAC ADDR: MACADDR2(highest), MACADDR1(middle), MACADDR0(lowest) */
#define YTPHY_WOL_MACADDR2_REG			0xA007
#define YTPHY_WOL_MACADDR1_REG			0xA008
#define YTPHY_WOL_MACADDR0_REG			0xA009

#define YTPHY_WOL_CONFIG_REG			0xA00A
#define YTPHY_WCR_INTR_SEL			BIT(6)
#define YTPHY_WCR_ENABLE			BIT(3)

/* 2b00 84ms
 * 2b01 168ms  *default*
 * 2b10 336ms
 * 2b11 672ms
 */
#define YTPHY_WCR_PULSE_WIDTH_MASK		(BIT(2) | BIT(1))
#define YTPHY_WCR_PULSE_WIDTH_672MS		(BIT(2) | BIT(1))

/* 1b0 Interrupt and WOL events is level triggered and active LOW  *default*
 * 1b1 Interrupt and WOL events is pulse triggered and active LOW
 */
#define YTPHY_WCR_TYPE_PULSE			BIT(0)

#define YTPHY_PAD_DRIVE_STRENGTH_REG		0xA010
#define YT8531_RGMII_RXC_DS_MASK		GENMASK(15, 13)
#define YT8531_RGMII_RXD_DS_HI_MASK		BIT(12)		/* Bit 2 of rxd_ds */
#define YT8531_RGMII_RXD_DS_LOW_MASK		GENMASK(5, 4)	/* Bit 1/0 of rxd_ds */
#define YT8531_RGMII_RX_DS_DEFAULT		0x3

#define YTPHY_SYNCE_CFG_REG			0xA012
#define YT8521_SCR_SYNCE_ENABLE			BIT(5)
/* 1b0 output 25m clock
 * 1b1 output 125m clock  *default*
 */
#define YT8521_SCR_CLK_FRE_SEL_125M		BIT(3)
#define YT8521_SCR_CLK_SRC_MASK			GENMASK(2, 1)
#define YT8521_SCR_CLK_SRC_PLL_125M		0
#define YT8521_SCR_CLK_SRC_UTP_RX		1
#define YT8521_SCR_CLK_SRC_SDS_RX		2
#define YT8521_SCR_CLK_SRC_REF_25M		3
#define YT8531_SCR_SYNCE_ENABLE			BIT(6)
/* 1b0 output 25m clock   *default*
 * 1b1 output 125m clock
 */
#define YT8531_SCR_CLK_FRE_SEL_125M		BIT(4)
#define YT8531_SCR_CLK_SRC_MASK			GENMASK(3, 1)
#define YT8531_SCR_CLK_SRC_PLL_125M		0
#define YT8531_SCR_CLK_SRC_UTP_RX		1
#define YT8531_SCR_CLK_SRC_SDS_RX		2
#define YT8531_SCR_CLK_SRC_CLOCK_FROM_DIGITAL	3
#define YT8531_SCR_CLK_SRC_REF_25M		4
#define YT8531_SCR_CLK_SRC_SSC_25M		5

#define YT8821_SDS_EXT_CSR_CTRL_REG			0x23
#define YT8821_SDS_EXT_CSR_VCO_LDO_EN			BIT(15)
#define YT8821_SDS_EXT_CSR_VCO_BIAS_LPF_EN		BIT(8)

#define YT8821_UTP_EXT_PI_CTRL_REG			0x56
#define YT8821_UTP_EXT_PI_RST_N_FIFO			BIT(5)
#define YT8821_UTP_EXT_PI_TX_CLK_SEL_AFE		BIT(4)
#define YT8821_UTP_EXT_PI_RX_CLK_3_SEL_AFE		BIT(3)
#define YT8821_UTP_EXT_PI_RX_CLK_2_SEL_AFE		BIT(2)
#define YT8821_UTP_EXT_PI_RX_CLK_1_SEL_AFE		BIT(1)
#define YT8821_UTP_EXT_PI_RX_CLK_0_SEL_AFE		BIT(0)

#define YT8821_UTP_EXT_VCT_CFG6_CTRL_REG		0x97
#define YT8821_UTP_EXT_FECHO_AMP_TH_HUGE		GENMASK(15, 8)

#define YT8821_UTP_EXT_ECHO_CTRL_REG			0x336
#define YT8821_UTP_EXT_TRACE_LNG_GAIN_THR_1000		GENMASK(14, 8)

#define YT8821_UTP_EXT_GAIN_CTRL_REG			0x340
#define YT8821_UTP_EXT_TRACE_MED_GAIN_THR_1000		GENMASK(6, 0)

#define YT8821_UTP_EXT_RPDN_CTRL_REG			0x34E
#define YT8821_UTP_EXT_RPDN_BP_FFE_LNG_2500		BIT(15)
#define YT8821_UTP_EXT_RPDN_BP_FFE_SHT_2500		BIT(7)
#define YT8821_UTP_EXT_RPDN_IPR_SHT_2500		GENMASK(6, 0)

#define YT8821_UTP_EXT_TH_20DB_2500_CTRL_REG		0x36A
#define YT8821_UTP_EXT_TH_20DB_2500			GENMASK(15, 0)

#define YT8821_UTP_EXT_TRACE_CTRL_REG			0x372
#define YT8821_UTP_EXT_TRACE_LNG_GAIN_THE_2500		GENMASK(14, 8)
#define YT8821_UTP_EXT_TRACE_MED_GAIN_THE_2500		GENMASK(6, 0)

#define YT8821_UTP_EXT_ALPHA_IPR_CTRL_REG		0x374
#define YT8821_UTP_EXT_ALPHA_SHT_2500			GENMASK(14, 8)
#define YT8821_UTP_EXT_IPR_LNG_2500			GENMASK(6, 0)

#define YT8821_UTP_EXT_PLL_CTRL_REG			0x450
#define YT8821_UTP_EXT_PLL_SPARE_CFG			GENMASK(7, 0)

#define YT8821_UTP_EXT_DAC_IMID_CH_2_3_CTRL_REG		0x466
#define YT8821_UTP_EXT_DAC_IMID_CH_3_10_ORG		GENMASK(14, 8)
#define YT8821_UTP_EXT_DAC_IMID_CH_2_10_ORG		GENMASK(6, 0)

#define YT8821_UTP_EXT_DAC_IMID_CH_0_1_CTRL_REG		0x467
#define YT8821_UTP_EXT_DAC_IMID_CH_1_10_ORG		GENMASK(14, 8)
#define YT8821_UTP_EXT_DAC_IMID_CH_0_10_ORG		GENMASK(6, 0)

#define YT8821_UTP_EXT_DAC_IMSB_CH_2_3_CTRL_REG		0x468
#define YT8821_UTP_EXT_DAC_IMSB_CH_3_10_ORG		GENMASK(14, 8)
#define YT8821_UTP_EXT_DAC_IMSB_CH_2_10_ORG		GENMASK(6, 0)

#define YT8821_UTP_EXT_DAC_IMSB_CH_0_1_CTRL_REG		0x469
#define YT8821_UTP_EXT_DAC_IMSB_CH_1_10_ORG		GENMASK(14, 8)
#define YT8821_UTP_EXT_DAC_IMSB_CH_0_10_ORG		GENMASK(6, 0)

#define YT8821_UTP_EXT_MU_COARSE_FR_CTRL_REG		0x4B3
#define YT8821_UTP_EXT_MU_COARSE_FR_F_FFE		GENMASK(14, 12)
#define YT8821_UTP_EXT_MU_COARSE_FR_F_FBE		GENMASK(10, 8)

#define YT8821_UTP_EXT_MU_FINE_FR_CTRL_REG		0x4B5
#define YT8821_UTP_EXT_MU_FINE_FR_F_FFE			GENMASK(14, 12)
#define YT8821_UTP_EXT_MU_FINE_FR_F_FBE			GENMASK(10, 8)

#define YT8821_UTP_EXT_VGA_LPF1_CAP_CTRL_REG		0x4D2
#define YT8821_UTP_EXT_VGA_LPF1_CAP_OTHER		GENMASK(7, 4)
#define YT8821_UTP_EXT_VGA_LPF1_CAP_2500		GENMASK(3, 0)

#define YT8821_UTP_EXT_VGA_LPF2_CAP_CTRL_REG		0x4D3
#define YT8821_UTP_EXT_VGA_LPF2_CAP_OTHER		GENMASK(7, 4)
#define YT8821_UTP_EXT_VGA_LPF2_CAP_2500		GENMASK(3, 0)

#define YT8821_UTP_EXT_TXGE_NFR_FR_THP_CTRL_REG		0x660
#define YT8821_UTP_EXT_NFR_TX_ABILITY			BIT(3)
/* Extended Register  end */

#define YTPHY_DTS_OUTPUT_CLK_DIS		0
#define YTPHY_DTS_OUTPUT_CLK_25M		25000000
#define YTPHY_DTS_OUTPUT_CLK_125M		125000000

#define YT8821_CHIP_MODE_AUTO_BX2500_SGMII	0
#define YT8821_CHIP_MODE_FORCE_BX2500		1

#define YT8824_RSSR_SPACE_MASK			BIT(0)
#define YT8824_RSSR_USXGMII_SPACE		(0x1)
#define YT8824_RSSR_UTP_SPACE			(0x0)
#define YT8824_UTP_TEMPLATE_TEST_MODE1		0x1
#define YT8824_UTP_TEMPLATE_TEST_NORMAL		0x0
#define YT8824_UTP_TEST_MODE_M			GENMASK(15, 13)
#define YT8824_UTP_TEST_MODE(x)	FIELD_PREP(YT8824_UTP_TEST_MODE_M, (x))
#define YT8824_SDS_CFG_MIN_PRE_MASK		GENMASK(3, 0)
#define YT8824_SDS_EN_FILL_PRE			BIT(13)
#define YT8824_SDS_TX_PRE_PADDING		(0x7)

struct yt8521_priv {
	/* combo_advertising is used for case of YT8521 in combo mode,
	 * this means that yt8521 may work in utp or fiber mode which depends
	 * on which media is connected (YT8521_RSSR_TO_BE_ARBITRATED).
	 */
	__ETHTOOL_DECLARE_LINK_MODE_MASK(combo_advertising);

	/* YT8521_MODE_FIBER / YT8521_MODE_UTP / YT8521_MODE_POLL*/
	u8 polling_mode;
	u8 strap_mode; /* 8 working modes  */
	/* current reg page of yt8521 phy:
	 * YT8521_RSSR_UTP_SPACE
	 * YT8521_RSSR_FIBER_SPACE
	 * YT8521_RSSR_TO_BE_ARBITRATED
	 */
	u8 reg_page;
};

struct yt8824_shared_priv {
	unsigned int interface_mode;
	/* shared_lock used to UTPs operation isolation during swap reg space */
	struct mutex shared_lock;
};

/**
 * ytphy_read_ext() - read a PHY's extended register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to read
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns the value of regnum reg or negative error code
 */
static int ytphy_read_ext(struct phy_device *phydev, u16 regnum)
{
	int ret;

	ret = __phy_write(phydev, YTPHY_PAGE_SELECT, regnum);
	if (ret < 0)
		return ret;

	return __phy_read(phydev, YTPHY_PAGE_DATA);
}

/**
 * ytphy_read_ext_with_lock() - read a PHY's extended register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to read
 *
 * returns the value of regnum reg or negative error code
 */

/**
 * ytphy_read_top_ext() - read a PHY's top extended register for YT8824
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to read
 *
 * Returns: the value of regnum reg or negative error code
 */
static int ytphy_read_top_ext(struct phy_device *phydev, u16 regnum)
{
	int ret;

	lockdep_assert_held(&phydev->mdio.bus->mdio_lock);
	ret = __phy_package_write(phydev, 0, YTPHY_PAGE_SELECT, regnum);
	if (ret < 0)
		return ret;

	return __phy_package_read(phydev, 0, YTPHY_PAGE_DATA);
}

/**
 * ytphy_write_top_ext() - write a PHY's top extended register for YT8824
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to write
 * @val: register val to write
 *
 * Returns: 0 or negative error code
 */
static int ytphy_write_top_ext(struct phy_device *phydev, u16 regnum,
			       u16 val)
{
	int ret;

	lockdep_assert_held(&phydev->mdio.bus->mdio_lock);
	ret = __phy_package_write(phydev, 0, YTPHY_PAGE_SELECT, regnum);
	if (ret < 0)
		return ret;

	return __phy_package_write(phydev, 0, YTPHY_PAGE_DATA, val);
}

/**
 * phy8824_page_write_with_lock() - write page for YT8824
 * @phydev: a pointer to a &struct phy_device
 * @page: reg page(YT8824_RSSR_USXGMII_SPACE/YT8824_RSSR_UTP_SPACE).
 *
 * Returns: 0 or negative error code
 */
static int phy8824_page_write_with_lock(struct phy_device *phydev, int page)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = ytphy_read_top_ext(phydev, YT8521_REG_SPACE_SELECT_REG);
	if (ret < 0)
		goto err;
	ret &= ~YT8824_RSSR_SPACE_MASK;
	ret |= (page & YT8824_RSSR_SPACE_MASK);
	ret = ytphy_write_top_ext(phydev, YT8521_REG_SPACE_SELECT_REG, ret);

err:
	phy_unlock_mdio_bus(phydev);
	return ret;
}

/**
 * ytphy_write_ext() - write a PHY's extended register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to write
 * @val: value to write to @regnum
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative error code
 */
static int ytphy_write_ext(struct phy_device *phydev, u16 regnum, u16 val)
{
	int ret;

	ret = __phy_write(phydev, YTPHY_PAGE_SELECT, regnum);
	if (ret < 0)
		return ret;

	return __phy_write(phydev, YTPHY_PAGE_DATA, val);
}

/**
 * ytphy_write_ext_with_lock() - write a PHY's extended register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to write
 * @val: value to write to @regnum
 *
 * returns 0 or negative error code
 */
static int ytphy_write_ext_with_lock(struct phy_device *phydev, u16 regnum,
				     u16 val)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = ytphy_write_ext(phydev, regnum, val);
	phy_unlock_mdio_bus(phydev);

	return ret;
}

/**
 * ytphy_modify_ext() - bits modify a PHY's extended register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to write
 * @mask: bit mask of bits to clear
 * @set: bit mask of bits to set
 *
 * NOTE: Convenience function which allows a PHY's extended register to be
 * modified as new register value = (old register value & ~mask) | set.
 * The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative error code
 */

/**
 * ytphy_modify_ext_with_lock() - bits modify a PHY's extended register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number to write
 * @mask: bit mask of bits to clear
 * @set: bit mask of bits to set
 *
 * NOTE: Convenience function which allows a PHY's extended register to be
 * modified as new register value = (old register value & ~mask) | set.
 *
 * returns 0 or negative error code
 */

/**
 * ytphy_get_wol() - report whether wake-on-lan is enabled
 * @phydev: a pointer to a &struct phy_device
 * @wol: a pointer to a &struct ethtool_wolinfo
 *
 * NOTE: YTPHY_WOL_CONFIG_REG is common ext reg.
 */

/**
 * ytphy_set_wol() - turn wake-on-lan on or off
 * @phydev: a pointer to a &struct phy_device
 * @wol: a pointer to a &struct ethtool_wolinfo
 *
 * NOTE: YTPHY_WOL_CONFIG_REG, YTPHY_WOL_MACADDR2_REG, YTPHY_WOL_MACADDR1_REG
 * and YTPHY_WOL_MACADDR0_REG are common ext reg. The
 * YTPHY_INTERRUPT_ENABLE_REG of UTP is special, fiber also use this register.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8824_read_page() - read PHY8824 reg page
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: current reg space of yt8824 (YT8824_RSSR_USXGMII_SPACE/
 * YT8824_RSSR_UTP_SPACE) or negative errno code
 */
static int yt8824_read_page(struct phy_device *phydev)
{
	int old_page;

	old_page = ytphy_read_top_ext(phydev, YT8521_REG_SPACE_SELECT_REG);
	if (old_page < 0)
		return old_page;

	return old_page & YT8824_RSSR_SPACE_MASK;
};

/**
 * yt8824_write_page() - write reg page
 * @phydev: a pointer to a &struct phy_device
 * @page: Reg page(YT8824_RSSR_USXGMII_SPACE/YT8824_RSSR_UTP_SPACE) to write.
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_write_page(struct phy_device *phydev, int page)
{
	int old_page;
	u16 data;

	old_page = ytphy_read_top_ext(phydev, YT8521_REG_SPACE_SELECT_REG);
	if (old_page < 0)
		return old_page;
	data = old_page & (~YT8824_RSSR_SPACE_MASK);
	data |= page;

	return ytphy_write_top_ext(phydev, YT8521_REG_SPACE_SELECT_REG, data);
};

/**
 * yt8824_utp_invalid_test_mode_paged() - config YT8824 to invalid test mode.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_utp_invalid_test_mode_paged(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	return genphy_c45_template_testmode
		(phydev, YT8824_UTP_TEMPLATE_TEST_MODE1);
}

/**
 * yt8824_sds_isolate_paged() - enable YT8824 serdes isolate.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_sds_isolate_paged(struct phy_device *phydev)
{
	int old_page = YT8824_RSSR_UTP_SPACE;
	int ret = 0;

	old_page = phy_select_page(phydev, YT8824_RSSR_USXGMII_SPACE);
	if (old_page < 0)
		goto err_restore_page;

	/* enable sds isolate */
	ret = __phy_modify(phydev, MII_BMCR, BMCR_ISOLATE, BMCR_ISOLATE);

err_restore_page:
	/* restore page, release the lock */
	return phy_restore_page(phydev, old_page, ret);
}

/**
 * yt8824_utp_softreset_paged() - config YT8824 UTP softreset.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_utp_softreset_paged(struct phy_device *phydev)
{
	int ret = 0;
	int val;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;
	ret = phy_modify(phydev, MII_BMCR, BMCR_RESET, BMCR_RESET);
	if (ret < 0)
		return ret;
	/* wait until softreset done. */
	return phy_read_poll_timeout(phydev, MII_BMCR, val,
				     !(val & BMCR_RESET),
				     50000, 600000, true);
}

/**
 * yt8824_utp_normal_test_mode_paged() - config YT8824 to normal test mode.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_utp_normal_test_mode_paged(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	return genphy_c45_template_testmode
		(phydev, YT8824_UTP_TEMPLATE_TEST_NORMAL);
}

/**
 * yt8824_sds_isolate_and_softreset_paged() - disable YT8824 serdes isolate
 * and sds softreset.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_sds_isolate_and_softreset_paged(struct phy_device *phydev)
{
	int old_page = YT8824_RSSR_UTP_SPACE;
	int val = 0;
	int ret = -1;

	old_page = phy_select_page(phydev, YT8824_RSSR_USXGMII_SPACE);
	if (old_page < 0)
		goto err_restore_page;

	/* sds softreset and disable isolate */
	ret = __phy_modify(phydev, MII_BMCR, BMCR_RESET | BMCR_ISOLATE,
			   BMCR_RESET & ~BMCR_ISOLATE);
	if (ret < 0)
		goto err_restore_page;

	/* poll while still holding the lock */
	ret = read_poll_timeout(__phy_read, val, (val < 0) ||
				!(val & BMCR_RESET),
				50000, 600000, true, phydev, MII_BMCR);
	if (val < 0)
		ret = val;

err_restore_page:
	/* restore page, release the lock */
	return phy_restore_page(phydev, old_page, ret);
}

/**
 * yt8824_soft_reset() - called to do PHY software reset
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_soft_reset(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret;

	mutex_lock(&priv->shared_lock);
	if (priv->interface_mode == PHY_INTERFACE_MODE_INTERNAL) {
		/* invalid test mode */
		ret = yt8824_utp_invalid_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;
		ret = yt8824_utp_softreset_paged(phydev);
		if (ret < 0)
			goto retry;
		/* normal mode */
		ret = yt8824_utp_normal_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;
	} else {
		/* invalid test mode */
		ret = yt8824_utp_invalid_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;

		/* sds isolation */
		ret = yt8824_sds_isolate_paged(phydev);
		if (ret < 0)
			goto retry;

		/* utp soft reset */
		ret = yt8824_utp_softreset_paged(phydev);
		if (ret < 0)
			goto retry;

		/* normal mode */
		ret = yt8824_utp_normal_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;

		/* sds soft reset and disable isolation */
		ret = yt8824_sds_isolate_and_softreset_paged(phydev);
		if (ret < 0)
			goto retry;
	}
	mutex_unlock(&priv->shared_lock);
	return ret;
retry:
	ret = yt8824_utp_normal_test_mode_paged(phydev);
	/* sds soft reset and disable isolation */
	ret |= yt8824_sds_isolate_and_softreset_paged(phydev);
	mutex_unlock(&priv->shared_lock);

	return ret;
}

/**
 * yt8824_extern_config_utp_init_paged() - config external phy8824 utp init
 * @phydev: target phy_device struct
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_extern_config_utp_init_paged(struct phy_device *phydev)
{
	int ret = 0;
	int val = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;
	/* power down */
	ret = phy_modify(phydev, MII_BMCR, BMCR_PDOWN, BMCR_PDOWN);
	if (ret < 0)
		return ret;

	/* pll calibration */
	ret = ytphy_write_ext_with_lock(phydev, 0x0001, 0x0003);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa20e, 0x0cba);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa20a, 0xc3f1);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa20c, 0x1620);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa2b6, 0x0a00);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa2b6, 0x0e00);
	if (ret < 0)
		return ret;

	/* optimization utp */
	ret = ytphy_write_ext_with_lock(phydev, 0x0001, 0x0003);
	if (ret < 0)
		return ret;

	/* enable nibble */
	ret = ytphy_write_ext_with_lock(phydev, 0xa003, 0x0003);
	if (ret < 0)
		return ret;

	/* idle err detect enable */
	ret = ytphy_write_ext_with_lock(phydev, 0x03d0, 0x5210);
	if (ret < 0)
		return ret;

	/* optimized 2.5G long cable performance */
	ret = ytphy_write_ext_with_lock(phydev, 0x0372, 0x5038);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x037c, 0x6068);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0388, 0x00a0);
	if (ret < 0)
		return ret;

	/* optimized fast retrain */
	ret = ytphy_write_ext_with_lock(phydev, 0x0359, 0x2140);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x000c, 0xc1a0);
	if (ret < 0)
		return ret;

	/* 2.5G template tone */
	ret = ytphy_write_ext_with_lock(phydev, 0xa2fa, 0x0083);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x04e2, 0x0149);
	if (ret < 0)
		return ret;

	/* optimized 2.5G template */
	ret = ytphy_write_ext_with_lock(phydev, 0x047e, 0x3939);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x047f, 0x3939);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0480, 0x3939);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0481, 0x3939);
	if (ret < 0)
		return ret;

	/* optimized 1000M cable length threshold */
	ret = ytphy_write_ext_with_lock(phydev, 0x0336, 0xab0a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0340, 0x301d);
	if (ret < 0)
		return ret;

	/* 100M template amplitude */
	ret = ytphy_write_ext_with_lock(phydev, 0x046e, 0x4545);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x046f, 0x4545);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0470, 0x4545);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0471, 0x4545);
	if (ret < 0)
		return ret;

	/* optimized 100M cable length threshold */
	ret = ytphy_write_ext_with_lock(phydev, 0x030b, 0xaa1d);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x071f, 0x0036);
	if (ret < 0)
		return ret;

	/* 10M template amplitude */
	ret = ytphy_write_ext_with_lock(phydev, 0x046b, 0x1818);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x046c, 0x1818);
	if (ret < 0)
		return ret;

	/* optimized 10M cable length threshold */
	ret = ytphy_write_ext_with_lock(phydev, 0x0466, 0x6c6c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0467, 0x6c6c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0468, 0x6c6c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0469, 0x6c6c);
	if (ret < 0)
		return ret;

	/* optimize utp 1000M performance */
	ret = ytphy_write_ext_with_lock(phydev, 0x034a, 0xff03);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x00f8, 0xb3ff);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0059, 0x4040);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x032c, 0x5094);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x032d, 0xd094);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x032e, 0x5308);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x0322, 0x6440);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x04d3, 0x5220);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x04d2, 0x5220);
	if (ret < 0)
		return ret;

	/* optimized EMC CS */
	ret = ytphy_write_ext_with_lock(phydev, 0x00c8, 0xffff);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x00be, 0x6406);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x037a, 0x40ff);
	if (ret < 0)
		return ret;

	/* optimized EMC RE */
	ret = ytphy_write_ext_with_lock(phydev, 0x0482, 0xffff);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa2d5, 0x1f1f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa2d6, 0x1f1f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa2d7, 0x1f1f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa2d8, 0x1f1f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa218, 0x006e);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa01d, 0xfff0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa01e, 0xfff0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa01d, 0xffff);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0xa01e, 0xffff);
	if (ret < 0)
		return ret;

	ret = genphy_c45_template_testmode
		(phydev, YT8824_UTP_TEMPLATE_TEST_MODE1);
	if (ret < 0)
		return ret;
	/* reset */
	ret = phy_modify(phydev, MII_BMCR, BMCR_RESET | BMCR_ANENABLE,
			 BMCR_RESET | BMCR_ANENABLE);
	if (ret < 0)
		return ret;
	ret = phy_read_poll_timeout(phydev, MII_BMCR, val,
				    !(val & BMCR_RESET),
				    50000, 600000, true);
	if (ret < 0)
		return ret;

	return genphy_c45_template_testmode
		(phydev, YT8824_UTP_TEMPLATE_TEST_NORMAL);
}

/**
 * yt8824_extern_config_sds_init_paged() - config external phy8824 sds init
 * @phydev: target phy_device struct
 *
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_extern_config_sds_init_paged(struct phy_device *phydev)
{
	int old_page = YT8824_RSSR_UTP_SPACE;
	int val_1, val_2, val_3, tmp;
	int val;
	int ret;

	old_page = phy_select_page(phydev, YT8824_RSSR_USXGMII_SPACE);
	if (old_page < 0)
		goto err_restore_page;

	/* read efuse */
	ret = ytphy_read_top_ext(phydev, 0xa13e);
	if (ret < 0)
		goto err_restore_page;
	else
		val_1 = ret;

	ret = ytphy_read_top_ext(phydev, 0xa13f);
	if (ret < 0)
		goto err_restore_page;
	else
		val_2 = ret;

	ret = ytphy_read_top_ext(phydev, 0xa140);
	if (ret < 0)
		goto err_restore_page;
	else
		val_3 = ret;

	/* Serdes optimization */
	ret = ytphy_write_ext(phydev, 0x04be, 0x000d);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x049f, 0x7ded);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x04a9, 0x009f);
	if (ret < 0)
		goto err_restore_page;

	/* analog CDR */
	ret = ytphy_write_ext(phydev, 0x0406, 0x0800);
	if (ret < 0)
		goto err_restore_page;

	/* optimized VCO */
	ret = ytphy_write_ext(phydev, 0x0438, 0x9024);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x0439, 0x00c0);
	if (ret < 0)
		goto err_restore_page;

	/* optimized PLL lock */
	ret = ytphy_read_ext(phydev, 0x0429);
	if (ret < 0)
		goto err_restore_page;

	ret &= ~(BIT(13) | BIT(12));
	tmp = (val_1 & (BIT(7) | BIT(6))) >> 6;
	ret |= (tmp << 12);
	ret = ytphy_write_ext(phydev, 0x0429, ret);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_read_ext(phydev, 0x0441);
	if (ret < 0)
		goto err_restore_page;

	ret &= ~(BIT(1) | BIT(0));
	tmp = (val_1 & (BIT(5) | BIT(4))) >> 4;
	ret |= tmp;
	ret = ytphy_write_ext(phydev, 0x0441, ret);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_read_ext(phydev, 0x042b);
	if (ret < 0)
		goto err_restore_page;

	ret &= ~(BIT(13) | BIT(12));
	tmp = (val_3 & (BIT(1) | BIT(0)));
	ret |= (tmp << 12);
	ret = ytphy_write_ext(phydev, 0x042b, ret);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x043a, 0x1006);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x042a, 0xf070);
	if (ret < 0)
		goto err_restore_page;

	/* cable length threshold */
	ret = ytphy_write_ext(phydev, 0x0491, 0x007f);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x0492, 0x7f7f);
	if (ret < 0)
		goto err_restore_page;

	/* Serdes training threshold */
	ret = ytphy_write_ext(phydev, 0x0454, 0x0f14);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x0497, 0x0a44);
	if (ret < 0)
		goto err_restore_page;

	/* digital eye diagram of SerDes */
	ret = ytphy_write_ext(phydev, 0x04cd, 0x0000);
	if (ret < 0)
		goto err_restore_page;

	/* Serdes LDO */
	ret = ytphy_read_ext(phydev, 0x04b5);
	if (ret < 0)
		goto err_restore_page;

	ret &= ~(BIT(6) | BIT(5) | BIT(4));
	tmp = (val_2 & (BIT(4) | BIT(3) | BIT(2))) >> 2;
	ret |= (tmp << 4);
	ret = ytphy_write_ext(phydev, 0x04b5, ret);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_read_ext(phydev, 0x04b4);
	if (ret < 0)
		goto err_restore_page;

	ret &= ~(BIT(10) | BIT(9) | BIT(8));
	tmp = (val_2 & (BIT(7) | BIT(6) | BIT(5))) >> 5;
	ret |= (tmp << 8);
	ret = ytphy_write_ext(phydev, 0x04b4, ret);
	if (ret < 0)
		goto err_restore_page;

	/* optimized Serdes RX */
	ret = ytphy_write_ext(phydev, 0x04af, 0x45e3);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x048a, 0x0fff);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x0408, 0x7c00);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x04d6, 0x007f);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x044f, 0xff08);
	if (ret < 0)
		goto err_restore_page;

	/* optimized Serdes TX */
	ret = ytphy_write_ext(phydev, 0x048e, 0x7d00);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x000d, 0x0606);
	if (ret < 0)
		goto err_restore_page;

	/* Serdes manual config */
	ret = ytphy_write_ext(phydev, 0x04b0, 0x0804);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x04b1, 0x7074);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x04af, 0x45e7);
	if (ret < 0)
		goto err_restore_page;

	/* restart calibration */
	ret = ytphy_write_ext(phydev, 0x0003, 0x5603);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x0492, 0x7fff);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x0492, 0x7f7f);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x2000, 0x0040);
	if (ret < 0)
		goto err_restore_page;

	ret = ytphy_write_ext(phydev, 0x2000, 0x0000);
	if (ret < 0)
		goto err_restore_page;

	/* TX preamble padded to 8; RX IPG always > 8 */
	ret = __phy_read(phydev, MII_RESV1);
	if (ret < 0)
		goto err_restore_page;
	ret &= ~YT8824_SDS_CFG_MIN_PRE_MASK;
	ret |= YT8824_SDS_TX_PRE_PADDING;
	ret |= YT8824_SDS_EN_FILL_PRE;
	ret = __phy_write(phydev, MII_RESV1, ret);
	if (ret < 0)
		goto err_restore_page;
	/* reset serdes */
	ret = __phy_modify(phydev, MII_BMCR, BMCR_RESET | BMCR_ANENABLE,
			   BMCR_RESET | BMCR_ANENABLE);
	if (ret < 0)
		goto err_restore_page;
	/* poll while still holding the lock; __phy_read takes no lock */
	ret = read_poll_timeout(__phy_read, val, (val < 0) ||
				!(val & BMCR_RESET),
				50000, 600000, true, phydev, MII_BMCR);
	if (val < 0)
		ret = val;
err_restore_page:
	/* restore page, release the lock */
	return phy_restore_page(phydev, old_page, ret);
}

/**
 * yt8824_internal_config_init_paged() - config internal phy8824 init
 * @phydev: target phy_device struct
 *
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_internal_config_init_paged(struct phy_device *phydev)
{
	int ret = 0;
	int val = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext_with_lock(phydev, 0x1, 0x3);
	if (ret < 0)
		return ret;
	/* power down */
	ret = phy_modify(phydev, MII_BMCR, BMCR_PDOWN, BMCR_PDOWN);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa20e, 0xcba);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa20a, 0xc3f1);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa20c, 0x1620);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2b6, 0xa00);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2b6, 0xe00);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa003, 0x3);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x3d0, 0x5210);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x372, 0x5038);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x37c, 0x6068);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x388, 0xa0);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x359, 0x2140);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2fa, 0x83);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x4e2, 0x149);
	if (ret < 0)
		return ret;
	/* 2.5G tempate */
	ret = ytphy_write_ext_with_lock(phydev, 0x47e, 0x3939);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x47f, 0x3939);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x480, 0x3939);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x481, 0x3939);
	if (ret < 0)
		return ret;
	/* 1000 cable length threshold */
	ret = ytphy_write_ext_with_lock(phydev, 0x336, 0xab0a);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x340, 0x301d);
	if (ret < 0)
		return ret;
	/* 1000 performance */
	ret = ytphy_write_ext_with_lock(phydev, 0x34a, 0xff03);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xf8, 0xb3ff);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x32c, 0x5094);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x32d, 0xd094);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x32e, 0x5308);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x322, 0x6440);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x4d3, 0x5220);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x4d2, 0x5220);
	if (ret < 0)
		return ret;
	/* 100 tempate */
	ret = ytphy_write_ext_with_lock(phydev, 0x46e, 0x4545);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x46f, 0x4545);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x470, 0x4545);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x471, 0x4545);
	if (ret < 0)
		return ret;
	/* 100 cable length threshold */
	ret = ytphy_write_ext_with_lock(phydev, 0x30b, 0xaa1d);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x71f, 0x36);
	if (ret < 0)
		return ret;
	/* 10 tempate */
	ret = ytphy_write_ext_with_lock(phydev, 0x46b, 0x1818);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x46c, 0x1818);
	if (ret < 0)
		return ret;
	/* 10 tempate MAU*/
	ret = ytphy_write_ext_with_lock(phydev, 0x466, 0x6c6c);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x467, 0x6c6c);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x468, 0x6c6c);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x469, 0x6c6c);
	if (ret < 0)
		return ret;
	/* EMC CS, Inconsistent with external phy */
	ret = ytphy_write_ext_with_lock(phydev, 0xc8, 0xfff);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xbe, 0x6406);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0x37a, 0x40ff);
	if (ret < 0)
		return ret;
	/* EMC RE*/
	ret = ytphy_write_ext_with_lock(phydev, 0x482, 0xffff);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2d5, 0x1f1f);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2d6, 0x1f1f);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2d7, 0x1f1f);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa2d8, 0x1f1f);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa218, 0x6e);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa01d, 0xfff0);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa01e, 0xfff0);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa01d, 0xffff);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xa01e, 0xffff);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext_with_lock(phydev, 0xc, 0x41a1);
	if (ret < 0)
		return ret;
	ret = genphy_c45_template_testmode
		(phydev, YT8824_UTP_TEMPLATE_TEST_MODE1);
	if (ret)
		return ret;
	/* reset */
	ret = phy_modify(phydev, MII_BMCR, BMCR_RESET | BMCR_ANENABLE,
			 BMCR_RESET | BMCR_ANENABLE);
	if (ret < 0)
		return ret;
	ret = phy_read_poll_timeout(phydev, MII_BMCR, val,
				    !(val & BMCR_RESET),
				    50000, 600000, true);
	if (ret)
		return ret;

	return genphy_c45_template_testmode
		(phydev, YT8824_UTP_TEMPLATE_TEST_NORMAL);
}

/**
 * yt8824_config_init() - phy initializatioin
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_config_init(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret;

	mutex_lock(&priv->shared_lock);
	if (priv->interface_mode == PHY_INTERFACE_MODE_INTERNAL) {
		ret = yt8824_internal_config_init_paged(phydev);
		if (ret < 0)
			goto err;
	} else {
		ret = yt8824_extern_config_sds_init_paged(phydev);
		if (ret < 0)
			goto err;
		ret = yt8824_extern_config_utp_init_paged(phydev);
		if (ret < 0)
			goto err;
	}
	mutex_unlock(&priv->shared_lock);
	ret = yt8824_soft_reset(phydev);

	phydev_dbg(phydev, "%s done, phy addr: %d\n",
		   __func__, phydev->mdio.addr);
	return ret;
err:
	mutex_unlock(&priv->shared_lock);
	return ret;
}

/**
 * yt8521_read_page() - read reg page
 * @phydev: a pointer to a &struct phy_device
 *
 * returns current reg space of yt8521 (YT8521_RSSR_FIBER_SPACE/
 * YT8521_RSSR_UTP_SPACE) or negative errno code
 */

/**
 * struct ytphy_ldo_vol_map - map a current value to a register value
 * @vol: ldo voltage
 * @ds:  value in the register
 * @cur: value in device configuration
 */
struct ytphy_ldo_vol_map {
	u32 vol;
	u32 ds;
	u32 cur;
};

/**
 * yt8521_probe() - read chip config then set suitable polling_mode
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */

/**
 * ytphy_utp_read_lpa() - read LPA then setup lp_advertising for utp
 * @phydev: a pointer to a &struct phy_device
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_adjust_status() - update speed and duplex to phydev. when in fiber
 * mode, adjust speed and duplex.
 * @phydev: a pointer to a &struct phy_device
 * @status: yt8521 status read from YTPHY_SPECIFIC_STATUS_REG
 * @is_utp: false(yt8521 work in fiber mode) or true(yt8521 work in utp mode)
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns 0
 */

/**
 * yt8521_read_status_paged() -  determines the speed and duplex of one page
 * @phydev: a pointer to a &struct phy_device
 * @page: The reg page(YT8521_RSSR_FIBER_SPACE/YT8521_RSSR_UTP_SPACE) to
 * operate.
 *
 * returns 1 (utp or fiber link),0 (no link) or negative errno code
 */

/**
 * yt8521_read_status() -  determines the negotiated speed and duplex
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_modify_bmcr_paged - bits modify a PHY's BMCR register of one page
 * @phydev: the phy_device struct
 * @page: The reg page(YT8521_RSSR_FIBER_SPACE/YT8521_RSSR_UTP_SPACE) to operate
 * @mask: bit mask of bits to clear
 * @set: bit mask of bits to set
 *
 * NOTE: Convenience function which allows a PHY's BMCR register to be
 * modified as new register value = (old register value & ~mask) | set.
 * YT8521 has two space (utp/fiber) and three mode (utp/fiber/poll), each space
 * has MII_BMCR. poll mode combines utp and faber,so need do both.
 * If it is reset, it will wait for completion.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_modify_utp_fiber_bmcr - bits modify a PHY's BMCR register
 * @phydev: the phy_device struct
 * @mask: bit mask of bits to clear
 * @set: bit mask of bits to set
 *
 * NOTE: Convenience function which allows a PHY's BMCR register to be
 * modified as new register value = (old register value & ~mask) | set.
 * YT8521 has two space (utp/fiber) and three mode (utp/fiber/poll), each space
 * has MII_BMCR. poll mode combines utp and faber,so need do both.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_soft_reset() - called to issue a PHY software reset
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_suspend() - suspend the hardware
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_resume() - resume the hardware
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_config_init() - called to initialize the PHY
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */


/**
 * yt8531_link_change_notify() - Adjust the tx clock direction according to
 * the current speed and dts config.
 * @phydev: a pointer to a &struct phy_device
 *
 * NOTE: This function is only used to adapt to VF2 with JH7110 SoC. Please
 * keep "motorcomm,tx-clk-adj-enabled" not exist in dts when the soc is not
 * JH7110.
 */

/**
 * yt8521_prepare_fiber_features() -  A small helper function that setup
 * fiber's features.
 * @phydev: a pointer to a &struct phy_device
 * @dst: a pointer to store fiber's features
 */

/**
 * yt8521_fiber_setup_forced - configures/forces speed from @phydev
 * @phydev: target phy_device struct
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * ytphy_check_and_restart_aneg - Enable and restart auto-negotiation
 * @phydev: target phy_device struct
 * @restart: whether aneg restart is requested
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_fiber_config_aneg - restart auto-negotiation or write
 * YTPHY_MISC_CONFIG_REG.
 * @phydev: target phy_device struct
 *
 * NOTE:The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * ytphy_setup_master_slave
 * @phydev: target phy_device struct
 *
 * NOTE: The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * ytphy_utp_config_advert - sanitize and advertise auto-negotiation parameters
 * @phydev: target phy_device struct
 *
 * NOTE: Writes MII_ADVERTISE with the appropriate values,
 * after sanitizing the values to make sure we only advertise
 * what is supported.  Returns < 0 on error, 0 if the PHY's advertisement
 * hasn't changed, and > 0 if it has changed.
 * The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * ytphy_utp_config_aneg - restart auto-negotiation or write BMCR
 * @phydev: target phy_device struct
 * @changed: whether autoneg is requested
 *
 * NOTE: If auto-negotiation is enabled, we configure the
 * advertising, and then restart auto-negotiation.  If it is not
 * enabled, then we write the BMCR.
 * The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_config_aneg_paged() - switch reg space then call genphy_config_aneg
 * of one page
 * @phydev: a pointer to a &struct phy_device
 * @page: The reg page(YT8521_RSSR_FIBER_SPACE/YT8521_RSSR_UTP_SPACE) to
 * operate.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_config_aneg() - change reg space then call yt8521_config_aneg_paged
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_aneg_done_paged() - determines the auto negotiation result of one
 * page.
 * @phydev: a pointer to a &struct phy_device
 * @page: The reg page(YT8521_RSSR_FIBER_SPACE/YT8521_RSSR_UTP_SPACE) to
 * operate.
 *
 * returns 0(no link)or 1(fiber or utp link) or negative errno code
 */

/**
 * yt8521_aneg_done() - determines the auto negotiation result
 * @phydev: a pointer to a &struct phy_device
 *
 * returns 0(no link)or 1(fiber or utp link) or negative errno code
 */

/**
 * ytphy_utp_read_abilities - read PHY abilities from Clause 22 registers
 * @phydev: target phy_device struct
 *
 * NOTE: Reads the PHY's abilities and populates
 * phydev->supported accordingly.
 * The caller must have taken the MDIO bus lock.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_get_features_paged() -  read supported link modes for one page
 * @phydev: a pointer to a &struct phy_device
 * @page: The reg page(YT8521_RSSR_FIBER_SPACE/YT8521_RSSR_UTP_SPACE) to
 * operate.
 *
 * returns 0 or negative errno code
 */

/**
 * yt8521_get_features - switch reg space then call yt8521_get_features_paged
 * @phydev: target phy_device struct
 *
 * returns 0 or negative errno code
 */

/**
 * yt8821_get_features - read mmd register to get 2.5G capability
 * @phydev: target phy_device struct
 *
 * Returns: 0 or negative errno code
 */
static int yt8821_get_features(struct phy_device *phydev)
{
	int ret;

	ret = genphy_c45_pma_read_ext_abilities(phydev);
	if (ret < 0)
		return ret;

	return genphy_read_abilities(phydev);
}

/**
 * yt8821_get_rate_matching - read register to get phy chip mode
 * @phydev: target phy_device struct
 * @iface: PHY data interface type
 *
 * Returns: rate matching type or negative errno code
 */

/**
 * yt8821_aneg_done() - determines the auto negotiation result
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0(no link)or 1(utp link) or negative errno code
 */

/**
 * yt8821_serdes_init() - serdes init
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_utp_init() - utp init
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_auto_sleep_config() - phy auto sleep config
 * @phydev: a pointer to a &struct phy_device
 * @enable: true enable auto sleep, false disable auto sleep
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_soft_reset() - soft reset utp and serdes
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_config_init() - phy initializatioin
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_adjust_status() - update speed and duplex to phydev
 * @phydev: a pointer to a &struct phy_device
 * @val: read from YTPHY_SPECIFIC_STATUS_REG
 */
static void yt8821_adjust_status(struct phy_device *phydev, int val)
{
	int speed, duplex;
	int speed_mode;

	duplex = FIELD_GET(YTPHY_SSR_DUPLEX, val);
	speed_mode = val & YTPHY_SSR_SPEED_MASK;
	switch (speed_mode) {
	case YTPHY_SSR_SPEED_10M:
		speed = SPEED_10;
		break;
	case YTPHY_SSR_SPEED_100M:
		speed = SPEED_100;
		break;
	case YTPHY_SSR_SPEED_1000M:
		speed = SPEED_1000;
		break;
	case YTPHY_SSR_SPEED_2500M:
		speed = SPEED_2500;
		break;
	default:
		speed = SPEED_UNKNOWN;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;
}

/**
 * yt8821_update_interface() - update interface per current speed
 * @phydev: a pointer to a &struct phy_device
 */

/**
 * yt8821_read_status() -  determines the negotiated speed and duplex
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_modify_utp_fiber_bmcr - bits modify a PHY's BMCR register
 * @phydev: the phy_device struct
 * @mask: bit mask of bits to clear
 * @set: bit mask of bits to set
 *
 * NOTE: Convenience function which allows a PHY's BMCR register to be
 * modified as new register value = (old register value & ~mask) | set.
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_suspend() - suspend the hardware
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8821_resume() - resume the hardware
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */

/**
 * yt8824_get_features - read mmd register to get 2.5G capability
 * @phydev: target phy_device struct
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_get_features(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret;

	mutex_lock(&priv->shared_lock);
	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		goto err;
	ret = yt8821_get_features(phydev);

err:
	mutex_unlock(&priv->shared_lock);
	return ret;
}

/**
 * yt8824_aneg_done()  - check negotiation state.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: link status or negative errno code
 */
static int yt8824_aneg_done(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int link = 0;
	int ret = 0;

	mutex_lock(&priv->shared_lock);
	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		goto err;

	ret = phy_read(phydev, YTPHY_SPECIFIC_STATUS_REG);
	if (ret < 0)
		goto err;
	mutex_unlock(&priv->shared_lock);
	link = !!(ret & YTPHY_SSR_LINK);

	phydev_dbg(phydev, "%s, phy addr: %d, link_utp: %d\n",
		   __func__, phydev->mdio.addr, link);
	return link;
err:
	mutex_unlock(&priv->shared_lock);
	return ret;
}

/**
 * yt8824_read_status_paged() -  determines the speed and duplex of one page
 * @phydev: a pointer to a &struct phy_device
 * @status: The link status, include speed, duplex and link state.
 * @lpa: link partner advertising.
 *
 * Returns: 1 (utp link),0 (no link) or negative errno code
 */
static int yt8824_read_status_paged(struct phy_device *phydev,
				    int *status, int *lpa)
{
	int ret = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	ret = phy_read(phydev, MII_LPA);
	*lpa = ret;
	if (ret < 0)
		return ret;

	ret = phy_read(phydev, YTPHY_SPECIFIC_STATUS_REG);
	*status = ret;
	if (ret < 0)
		return ret;

	ret = !!(*status & YTPHY_SSR_LINK);

	return ret;
}

/**
 * yt8824_read_status() -  determines the negotiated speed and duplex
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_read_status(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int link;
	int lpa;
	int val;

	phydev->pause = 0;
	phydev->asym_pause = 0;
	phydev->link = 0;
	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;

	mutex_lock(&priv->shared_lock);
	link = yt8824_read_status_paged(phydev,
					&val, &lpa);
	mutex_unlock(&priv->shared_lock);
	if (link < 0)
		return link;

	if (link) {
		phydev->link = 1;
		phydev->pause = !!(lpa & BIT(10));
		phydev->asym_pause = !!(lpa & BIT(11));

		/* update speed & duplex */
		yt8821_adjust_status(phydev, val);
	} else {
		phydev->link = 0;
		phydev->pause = 0;
		phydev->asym_pause = 0;
		phydev->speed = SPEED_UNKNOWN;
		phydev->duplex = DUPLEX_UNKNOWN;
	}

	return 0;
}

/**
 * yt8824_utp_power_on(): utp power on.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_utp_power_on(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	return phy_modify(phydev, MII_BMCR, BMCR_PDOWN | BMCR_ISOLATE, 0x0);
}

/**
 * yt8824_utp_power_down(): utp power down.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_utp_power_down(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	return phy_modify(phydev, MII_BMCR, BMCR_PDOWN, BMCR_PDOWN);
}

/**
 * yt8824_power_on()  - set utp power on.
 * @phydev: a pointer to a &struct phy_device
 *
 * NOTE: need WA like softreset
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_power_on(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret;

	if (priv->interface_mode == PHY_INTERFACE_MODE_INTERNAL) {
		/* invalid test mode */
		ret = yt8824_utp_invalid_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;
		/* utp power on */
		ret = yt8824_utp_power_on(phydev);
		if (ret < 0)
			goto retry;
		/* normal mode */
		ret = yt8824_utp_normal_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;
	} else {
		/* invalid test mode */
		ret = yt8824_utp_invalid_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;

		/* sds isolation */
		ret = yt8824_sds_isolate_paged(phydev);
		if (ret < 0)
			goto retry;

		/* utp power on */
		ret = yt8824_utp_power_on(phydev);
		if (ret < 0)
			goto retry;

		/* normal mode */
		ret = yt8824_utp_normal_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;

		/* sds soft reset and disable isolation */
		ret = yt8824_sds_isolate_and_softreset_paged(phydev);
		if (ret < 0)
			goto retry;
	}
	return 0;

retry:
	/* retry configure normal mode */
	ret = yt8824_utp_normal_test_mode_paged(phydev);
	/* retry configure soft reset and disable isolation */
	ret |= yt8824_sds_isolate_and_softreset_paged(phydev);

	return ret;
}

/**
 * yt8824_resume() - resume the hardware
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_resume(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret = 0;

	mutex_lock(&priv->shared_lock);
	ret = yt8824_power_on(phydev);
	mutex_unlock(&priv->shared_lock);

	return ret;
}

/**
 * yt8824_power_down()  - set utp power down.
 * @phydev: a pointer to a &struct phy_device
 *
 * NOTE: need WA like softreset
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_power_down(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret;

	if (priv->interface_mode == PHY_INTERFACE_MODE_INTERNAL) {
		/* invalid test mode */
		ret = yt8824_utp_invalid_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;
		/* utp power down */
		ret = yt8824_utp_power_down(phydev);
		if (ret < 0)
			goto retry;
		/* normal mode */
		ret = yt8824_utp_normal_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;
	} else {
		/* invalid test mode */
		ret = yt8824_utp_invalid_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;

		/* sds isolation */
		ret = yt8824_sds_isolate_paged(phydev);
		if (ret < 0)
			goto retry;

		/* utp power down */
		ret = yt8824_utp_power_down(phydev);
		if (ret < 0)
			goto retry;

		/* normal mode */
		ret = yt8824_utp_normal_test_mode_paged(phydev);
		if (ret < 0)
			goto retry;

		/* sds soft reset and disable isolation */
		ret = yt8824_sds_isolate_and_softreset_paged(phydev);
		if (ret < 0)
			goto retry;
	}
	return 0;

retry:
	/* retry configure normal mode */
	ret = yt8824_utp_normal_test_mode_paged(phydev);
	/* retry configure soft reset and disable isolation */
	ret |= yt8824_sds_isolate_and_softreset_paged(phydev);

	return ret;
}

/**
 * yt8824_suspend() - suspend the hardware
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_suspend(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int ret = 0;

	mutex_lock(&priv->shared_lock);
	ret = yt8824_power_down(phydev);
	mutex_unlock(&priv->shared_lock);

	return ret;
}

/**
 * yt8824_config_aneg() - config negotiation
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_config_aneg(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	int phy_ctrl = 0;
	int ret = 0;

	mutex_lock(&priv->shared_lock);
	ret = phy8824_page_write_with_lock(phydev, YT8824_RSSR_UTP_SPACE);
	if (ret < 0)
		return ret;

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
			      phydev->advertising))
		phy_ctrl = MDIO_AN_10GBT_CTRL_ADV2_5G;

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN,
				     MDIO_AN_10GBT_CTRL,
				     MDIO_AN_10GBT_CTRL_ADV2_5G,
				     phy_ctrl);
	if (ret < 0)
		return ret;

	ret = __genphy_config_aneg(phydev, ret);
	if (ret < 0)
		return ret;

	mutex_unlock(&priv->shared_lock);
	return ret;
}

/**
 * yt8824_phy_package_probe_once()  - init phy packet for phy8824.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_phy_package_probe_once(struct phy_device *phydev)
{
	struct yt8824_shared_priv *priv = phy_package_get_priv(phydev);
	struct device_node *np = phy_package_get_node(phydev);
	const char *interface_mode_name;

	/* Initialise shared lock for YT8824 */
	mutex_init(&priv->shared_lock);
	priv->interface_mode = PHY_INTERFACE_MODE_INTERNAL;
	if (!of_property_read_string(np, "motorcomm,interface-mode",
				     &interface_mode_name)) {
		if (!strcasecmp(interface_mode_name,
				phy_modes(PHY_INTERFACE_MODE_USXGMII))) {
			priv->interface_mode = PHY_INTERFACE_MODE_USXGMII;
		} else if (!strcasecmp
				(interface_mode_name,
				 phy_modes(PHY_INTERFACE_MODE_INTERNAL))) {
			priv->interface_mode = PHY_INTERFACE_MODE_INTERNAL;
		} else {
			return -EINVAL;
		}
	} else {
		/* default internal phy */
		priv->interface_mode = PHY_INTERFACE_MODE_INTERNAL;
	}

	return 0;
}

/**
 * yt8824_probe() - phy8824 probe.
 * @phydev: a pointer to a &struct phy_device
 *
 * Returns: 0 or negative errno code
 */
static int yt8824_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct yt8824_shared_priv *shared_priv;
	int ret;

	ret = devm_of_phy_package_join(dev, phydev, sizeof(*shared_priv));
	if (ret)
		return ret;

	if (phy_package_probe_once(phydev)) {
		ret = yt8824_phy_package_probe_once(phydev);
		if (ret)
			return ret;
	}

	return 0;
}

static struct phy_driver yt8824_phy_drvs[] = {
	{
		PHY_ID_MATCH_EXACT(PHY_ID_YT8824),
		.name			= "YT8824 Quad Ports 2.5Gbps Ethernet",
		.get_features		= yt8824_get_features,
		.read_page		= yt8824_read_page,
		.write_page		= yt8824_write_page,
		.probe		        = yt8824_probe,
		.config_aneg		= yt8824_config_aneg,
		.aneg_done		= yt8824_aneg_done,
		.config_init		= yt8824_config_init,
		.read_status		= yt8824_read_status,
		.soft_reset		= yt8824_soft_reset,
		.suspend		= yt8824_suspend,
		.resume			= yt8824_resume,
	},
};

module_phy_driver(yt8824_phy_drvs);

static const struct mdio_device_id __maybe_unused yt8824_tbl[] = {
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8824) },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(mdio, yt8824_tbl);

MODULE_DESCRIPTION("Motorcomm YT8824 quad-port 2.5G PHY driver");
MODULE_AUTHOR("Kyle");
MODULE_LICENSE("GPL");
