#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=dsa-yt921x
PKG_RELEASE:=2
PKG_LICENSE:=GPL-2.0-or-later

include $(INCLUDE_DIR)/package.mk

define KernelPackage/dsa-yt921x
  SUBMENU:=Network Devices
  TITLE:=Motorcomm YT921x/YT922x DSA switch
  DEPENDS:=+kmod-dsa +kmod-phy-yt8824
  FILES:= \
	$(PKG_BUILD_DIR)/tag_yt921x.ko \
	$(PKG_BUILD_DIR)/tag_yt922x.ko \
	$(PKG_BUILD_DIR)/yt921x.ko
  AUTOLOAD:=$(call AutoLoad,40,tag_yt921x tag_yt922x yt921x,1)
endef

define KernelPackage/dsa-yt921x/description
 DSA driver and taggers for the Motorcomm YT921x and YT922x switch family,
 as found on the GL.iNet GL-BE14000 (YT9224). Built out of tree so the
 kernel config - and with it the vermagic the official 25.12 kmods are
 built against - stays untouched.

 This is JiaY-shi's complete YT9224 driver from blogic/openwrt#4 (eFuse
 calibration, SerDes/PCS, fixed 10G CPU trunk recovery, eight- and
 four-byte CPU tags, switch-level bridge forwarding and unicast FDB), with
 the API differences to 6.12 patched in the sources. Left out on purpose:
 multicast database and 802.1Q VLAN offload, whose fixes need new
 callbacks in struct dsa_switch_ops and would break the ABI of the
 official DSA kmods.
endef

define KernelPackage/phy-yt8824
  SUBMENU:=Network Devices
  TITLE:=Motorcomm YT8824 quad-port 2.5G PHY
  DEPENDS:=+kmod-libphy
  FILES:=$(PKG_BUILD_DIR)/yt8824.ko
  AUTOLOAD:=$(call AutoLoad,30,yt8824,1)
endef

define KernelPackage/phy-yt8824/description
 Driver for the Motorcomm YT8824 quad-port 2.5GbE PHY, the PHY package
 behind lan1-lan4 of the GL.iNet GL-BE14000. Cut out of the Motorcomm
 driver as patched by the net-next YT8824 series, so it does not clash
 with the official kmod-phy-motorcomm. It has to load before the switch
 driver registers its MDIO bus, or the generic PHY driver takes the ports.
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,phy-yt8824))
$(eval $(call KernelPackage,dsa-yt921x))
