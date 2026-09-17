#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=a73-ssb
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-or-later

include $(INCLUDE_DIR)/package.mk

define KernelPackage/a73-ssb
  SUBMENU:=Other modules
  TITLE:=Cortex-A73 speculative store bypass chicken bit
  DEPENDS:=@TARGET_mediatek_filogic @aarch64
  FILES:=$(PKG_BUILD_DIR)/a73_ssb.ko
endef

define KernelPackage/a73-ssb/description
 Reports the Cortex-A73 implementation defined bit that ARM Trusted Firmware
 sets as the CVE-2018-3639 mitigation, and clears it with clear=1. The bit
 costs roughly a factor of two in integer code, and the firmware hides it from
 the kernel by answering SMCCC_ARCH_WORKAROUND_2 with "not required".

 Not installed by default and not part of any image: the access is only
 permitted if EL3 grants it, and clearing the bit gives up the Spectre v4
 mitigation. Load it by hand to measure.
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,a73-ssb))
