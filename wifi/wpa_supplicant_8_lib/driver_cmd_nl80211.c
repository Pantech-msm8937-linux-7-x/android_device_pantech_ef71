/*
 * Android driver commands of wpa_supplicant, for wcn36xx (mac80211)
 *
 * Copyright (C) 2026 The LineageOS Project
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * The framework sends its private driver commands through these, which a
 * vendor library of the Wi-Fi chip handles. wcn36xx needs none of them but
 * the country code ("COUNTRY xx"), which is set through nl80211 here: with
 * no country, cfg80211 keeps the world regulatory domain, in which most of
 * 5 GHz is passive and connecting there fails.
 */

#include "includes.h"

#include "common.h"
#include "driver_nl80211.h"
#include "wpa_supplicant_i.h"
#include "linux_ioctl.h"

#define UNUSED_ARG __attribute__((__unused__))

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf, size_t buf_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = 0;

	if (os_strncasecmp(cmd, "COUNTRY ", 8) == 0) {
		ret = wpa_driver_nl80211_ops.set_country(priv, cmd + 8);
		wpa_printf(MSG_DEBUG, "nl80211: country %s: %d", cmd + 8, ret);
	} else if (os_strcasecmp(cmd, "MACADDR") == 0) {
		u8 macaddr[ETH_ALEN] = {};

		ret = linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname, macaddr);
		if (!ret)
			ret = os_snprintf(buf, buf_len, "Macaddr = " MACSTR "\n", MAC2STR(macaddr));
	}
	/* the other private commands: nothing to do for wcn36xx */

	return ret;
}

int wpa_driver_set_p2p_noa(UNUSED_ARG void *priv, UNUSED_ARG u8 count,
			   UNUSED_ARG int start, UNUSED_ARG int duration)
{
	return 0;
}

int wpa_driver_get_p2p_noa(UNUSED_ARG void *priv, UNUSED_ARG u8 *buf, UNUSED_ARG size_t len)
{
	return 0;
}

int wpa_driver_set_p2p_ps(UNUSED_ARG void *priv, UNUSED_ARG int legacy_ps,
			  UNUSED_ARG int opp_ps, UNUSED_ARG int ctwindow)
{
	return -1;
}

int wpa_driver_set_ap_wps_p2p_ie(UNUSED_ARG void *priv, UNUSED_ARG const struct wpabuf *beacon,
				 UNUSED_ARG const struct wpabuf *proberesp,
				 UNUSED_ARG const struct wpabuf *assocresp)
{
	return 0;
}
