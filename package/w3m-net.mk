####################################################################################
# w3m-net — network tools suite for the W3M desktop (Trinux-style diagnostics)
####################################################################################

W3M_NET_VERSION = v0.1.0
W3M_NET_SITE = $(call github,0ldskoolerz,w3m-net,$(W3M_NET_VERSION))
W3M_NET_LICENSE = MIT
W3M_NET_DEPENDENCIES = xlib_libX11 busybox

define W3M_NET_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define W3M_NET_INSTALL_TARGET_CMDS
	for bin in w3m-ping w3m-ifaces w3m-ports w3m-scan; do \
		$(INSTALL) -D -m 0755 $(@D)/$$bin $(TARGET_DIR)/usr/bin/$$bin; \
	done
endef

$(eval $(generic-package))
