/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Legacy Wi-Fi HAL for wcn36xx (mac80211), as the vendor HAL the default
 * Wi-Fi HAL service loads from /vendor/etc/wifi/vendor_hals.
 *
 * The framework sets the country code through the vendor HAL, and refuses to
 * start a 5 GHz hotspot without it; wcn36xx has no vendor HAL of its own.
 * This one sets the country through nl80211 (cfg80211 regulatory hint) and
 * lists the wireless interfaces. Whatever else the framework asks of a vendor
 * HAL stays with the stubs of the default HAL: "not supported".
 */

#define LOG_TAG "WifiHalEF71"

#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include <condition_variable>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#include <linux/nl80211.h>
#include <log/log.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <hardware_legacy/wifi_hal.h>

struct wifi_interface_info {
    std::string name;
};

struct wifi_info {
    std::mutex lock;
    std::condition_variable cv;
    bool stopping = false;
    wifi_cleaned_up_handler cleaned_up = nullptr;
    // Handles stay valid as long as the HAL runs: the default HAL keeps them
    std::list<wifi_interface_info> ifaces;
    std::vector<wifi_interface_handle> handles;
};

namespace {

bool isWireless(const char *name) {
    std::string path = std::string("/sys/class/net/") + name + "/phy80211";
    return access(path.c_str(), F_OK) == 0;
}

wifi_error initialize(wifi_handle *handle) {
    if (!handle) return WIFI_ERROR_INVALID_ARGS;
    *handle = new wifi_info();
    ALOGI("initialized");
    return WIFI_SUCCESS;
}

void cleanup(wifi_handle handle, wifi_cleaned_up_handler handler) {
    if (!handle) return;
    std::lock_guard<std::mutex> lock(handle->lock);
    handle->stopping = true;
    handle->cleaned_up = handler;
    handle->cv.notify_all();
}

void eventLoop(wifi_handle handle) {
    if (!handle) return;
    wifi_cleaned_up_handler handler;
    {
        std::unique_lock<std::mutex> lock(handle->lock);
        handle->cv.wait(lock, [handle] { return handle->stopping; });
        handler = handle->cleaned_up;
    }
    if (handler) handler(handle);
    delete handle;
    ALOGI("stopped");
}

wifi_error getIfaces(wifi_handle handle, int *num, wifi_interface_handle **interfaces) {
    if (!handle || !num || !interfaces) return WIFI_ERROR_INVALID_ARGS;
    std::lock_guard<std::mutex> lock(handle->lock);

    DIR *dir = opendir("/sys/class/net");
    if (!dir) return WIFI_ERROR_UNKNOWN;
    while (struct dirent *entry = readdir(dir)) {
        if (entry->d_name[0] == '.' || !isWireless(entry->d_name)) continue;
        bool known = false;
        for (const auto &iface : handle->ifaces) known |= iface.name == entry->d_name;
        if (!known) handle->ifaces.push_back({entry->d_name});
    }
    closedir(dir);

    handle->handles.clear();
    for (auto &iface : handle->ifaces) handle->handles.push_back(&iface);
    *num = handle->handles.size();
    *interfaces = handle->handles.data();
    return WIFI_SUCCESS;
}

wifi_error getIfaceName(wifi_interface_handle iface, char *name, size_t size) {
    if (!iface || !name || !size) return WIFI_ERROR_INVALID_ARGS;
    strlcpy(name, iface->name.c_str(), size);
    return WIFI_SUCCESS;
}

wifi_error getSupportedFeatureSet(wifi_interface_handle, feature_set *set) {
    if (!set) return WIFI_ERROR_INVALID_ARGS;
    *set = WIFI_FEATURE_INFRA | WIFI_FEATURE_INFRA_5G | WIFI_FEATURE_SOFT_AP;
    return WIFI_SUCCESS;
}

// The regulatory domain is global in cfg80211: set it through nl80211
wifi_error setCountryCode(wifi_interface_handle, const char *country) {
    if (!country || strlen(country) != 2) return WIFI_ERROR_INVALID_ARGS;

    struct nl_sock *sock = nl_socket_alloc();
    if (!sock) return WIFI_ERROR_OUT_OF_MEMORY;
    wifi_error ret = WIFI_ERROR_UNKNOWN;
    struct nl_msg *msg = nullptr;
    int family;

    if (genl_connect(sock) < 0) goto out;
    family = genl_ctrl_resolve(sock, "nl80211");
    if (family < 0) goto out;
    msg = nlmsg_alloc();
    if (!msg) goto out;
    if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, family, 0, 0,
                     NL80211_CMD_REQ_SET_REG, 0) ||
        nla_put_string(msg, NL80211_ATTR_REG_ALPHA2, country) < 0)
        goto out;
    if (nl_send_auto(sock, msg) < 0 || nl_wait_for_ack(sock) < 0) goto out;
    ret = WIFI_SUCCESS;

out:
    ALOGI("country %s: %d", country, ret);
    if (msg) nlmsg_free(msg);
    nl_socket_free(sock);
    return ret;
}

wifi_error getDriverVersion(wifi_interface_handle, char *buffer, int size) {
    if (!buffer || size <= 0) return WIFI_ERROR_INVALID_ARGS;
    strlcpy(buffer, "wcn36xx", size);
    return WIFI_SUCCESS;
}

}  // namespace

extern "C" wifi_error init_wifi_vendor_hal_func_table(wifi_hal_fn *fn) {
    if (!fn) return WIFI_ERROR_INVALID_ARGS;
    fn->wifi_initialize = initialize;
    fn->wifi_cleanup = cleanup;
    fn->wifi_event_loop = eventLoop;
    fn->wifi_get_ifaces = getIfaces;
    fn->wifi_get_iface_name = getIfaceName;
    fn->wifi_get_supported_feature_set = getSupportedFeatureSet;
    fn->wifi_set_country_code = setCountryCode;
    fn->wifi_get_driver_version = getDriverVersion;
    return WIFI_SUCCESS;
}
