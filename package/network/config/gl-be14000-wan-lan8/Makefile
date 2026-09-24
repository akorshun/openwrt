#
# Copyright (C) 2026 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.

include $(TOPDIR)/rules.mk

PKG_NAME:=gl-be14000-wan-lan8
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=Andrey Korshunov <razor1702@gmail.com>

include $(INCLUDE_DIR)/package.mk

define Package/gl-be14000-wan-lan8
  SECTION:=net
  CATEGORY:=Network
  TITLE:=GL-BE14000 default port layout with WAN on lan8
  DEPENDS:=@TARGET_mediatek_filogic
  PKGARCH:=all
endef

define Package/gl-be14000-wan-lan8/description
 Ships an /etc/board.d/ script that overrides the GL-BE14000 default network
 layout: the uplink is lan8 and the 10G RJ45 port stays in the LAN bridge,
 for installations whose internet connection is not faster than the 1G lan8
 port. Only the defaults change; the ports can be reassigned in LuCI either
 way.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/gl-be14000-wan-lan8/install
	$(INSTALL_DIR) $(1)/etc/board.d
	$(INSTALL_BIN) ./files/03_network-wan-lan8 $(1)/etc/board.d/03_network-wan-lan8
endef

$(eval $(call BuildPackage,gl-be14000-wan-lan8))
