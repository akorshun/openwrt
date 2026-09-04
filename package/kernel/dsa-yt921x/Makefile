#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=dsa-yt921x
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-or-later

include $(INCLUDE_DIR)/package.mk

define KernelPackage/dsa-yt921x
  SUBMENU:=Network Devices
  TITLE:=Motorcomm YT921x/YT922x DSA switch
  DEPENDS:=+kmod-dsa
  FILES:= \
	$(PKG_BUILD_DIR)/tag_yt921x.ko \
	$(PKG_BUILD_DIR)/yt921x.ko
  AUTOLOAD:=$(call AutoLoad,40,tag_yt921x yt921x,1)
endef

define KernelPackage/dsa-yt921x/description
 DSA driver and tagger for the Motorcomm YT921x and YT922x switch family,
 as found on the GL.iNet GL-BE14000 (YT9224). Built out of tree so the
 kernel config - and with it the vermagic the official 25.12 kmods are
 built against - stays untouched: this is the upstream yt921x driver at
 its v7.3 state plus the YT922x support from John Crispin's tree, with
 the few API differences to 6.12 patched in the sources.
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,dsa-yt921x))
