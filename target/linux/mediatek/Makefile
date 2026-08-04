# Copyright (c) 2015 OpenWrt.org
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=mediatek
BOARDNAME:=MediaTek ARM
SUBTARGETS:=filogic mt7622 mt7623 mt7629
FEATURES:=dt-overlay emmc fpu gpio nand pci pcie rootfs-part separate_ramdisk squashfs usb

KERNEL_PATCHVER:=6.12

include $(INCLUDE_DIR)/target.mk

# Force the kmod ABI stamp (vermagic) for filogic to match the value
# published for this release (LINUX_VERSION-LINUX_RELEASE-LINUX_VERMAGIC
# in targets/mediatek/filogic/kmods/ on downloads.openwrt.org), so kmod-*
# packages from the official 25.12.5 feed stay apk-installable on images
# built from this branch. A single-device build selects far fewer kernel
# modules than the buildbot does, and .vermagic is an md5 of the enabled
# kernel config symbols, so the computed hash would otherwise differ from
# the published one even though the kernel source is identical.
#
# A plain `LINUX_VERMAGIC:=...` override here only takes effect for
# Makefiles that include kernel.mk through *this* file's parse (i.e. the
# kernel/module build itself). Anything that includes kernel.mk on its
# own - e.g. the "kernel" meta-package, feeds.mk's distfeeds.list
# generation - re-derives LINUX_VERMAGIC fresh from $(LINUX_DIR)/.vermagic
# and never sees the override, so the two end up disagreeing (the
# "kernel" package reporting the real hash while bundled kmods report the
# forced one is an unsatisfiable apk world). Overwrite the .vermagic file
# itself instead, so every later `cat .vermagic` gets the same value
# regardless of which Makefile is asking.
ifeq ($(SUBTARGET),filogic)
define Kernel/Configure
	$(call Kernel/Configure/Default)
	echo 5a6c1f71be683ae9980b15d3ce73e24d > $(LINUX_DIR)/.vermagic
endef
endif

DEFAULT_PACKAGES += \
	kmod-leds-gpio kmod-gpio-button-hotplug

$(eval $(call BuildTarget))
