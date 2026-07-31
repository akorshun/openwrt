# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2013-2016 OpenWrt.org

include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=sunxi
BOARDNAME:=Allwinner ARM SoCs
FEATURES:=ext4 display ramdisk rootfs-part rtc squashfs usb usbgadget
SUBTARGETS:=cortexa8 cortexa7 cortexa53 arm926ejs

KERNEL_PATCHVER:=6.12

KERNELNAME:=zImage dtbs

# A10: Cortex-A8
# A13: Cortex-A8
# A20: dual Cortex-A7
# A31: quad Cortex-A7
# A80: octa Cortex-A15/A7
# H3: quad Cortex-A7
# R40: quad Cortex-A7
# A64/H5/H6/H616: quad Cortex-A53

include $(INCLUDE_DIR)/target.mk

# Force the kmod ABI stamp (vermagic) for cortexa53 to match the value
# published for this release (LINUX_VERSION-LINUX_RELEASE-LINUX_VERMAGIC
# in targets/sunxi/cortexa53/kmods/ on downloads.openwrt.org), so kmod-*
# packages built here stay apk-installable side by side with the
# official 25.12.5 feed for everything the A523 patches don't touch.
# The extra Kconfig symbols added for A523 boards would otherwise change
# the computed hash for the whole cortexa53 subtarget.
#
# A plain `LINUX_VERMAGIC:=...` override here only takes effect for
# Makefiles that include kernel.mk through *this* file's parse (i.e. the
# kernel/module build itself). Anything that includes kernel.mk on its
# own - e.g. the "kernel" meta-package, feeds.mk's distfeeds.list
# generation - re-derives LINUX_VERMAGIC fresh from $(LINUX_DIR)/.vermagic
# and never sees the override, so the two ended up disagreeing (the
# "kernel" package reported the real hash while bundled kmods reported
# the forced one - an unsatisfiable apk world). Overwrite the .vermagic
# file itself instead, so every later `cat .vermagic` gets the same
# value regardless of which Makefile is asking.
ifeq ($(SUBTARGET),cortexa53)
define Kernel/Configure
	$(Kernel/Configure/Default)
	echo 54c42a9af64da421c37305c250430d94 > $(LINUX_DIR)/.vermagic
endef
endif

DEFAULT_PACKAGES += uboot-envtools
DEFAULT_PACKAGES += partx-utils mkf2fs e2fsprogs

$(eval $(call BuildTarget))
