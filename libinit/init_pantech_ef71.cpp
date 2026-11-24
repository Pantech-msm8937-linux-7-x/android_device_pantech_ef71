/*
 * Copyright (C) 2021-2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include <android-base/file.h>

#include <libinit_dalvik_heap.h>
#include <libinit_utils.h>

#include "vendor_init.h"

/*
 * The panel, as apps such as DevCheck read it (the msm_fb_panel_info of the
 * downstream kernels, or this property): from the panel aboot names on the
 * command line, mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_<panel>:...
 */
static void set_panel_prop() {
    static const struct {
        const char *dsi_name;
        const char *display;
    } panels[] = {
        { "nt35695_1080p_cmd", "nt35695 1080p cmd mode dsi panel" },
        { "ft8716_1080p_video", "ft8716 1080p video mode dsi panel" },
    };
    std::string cmdline;

    if (!android::base::ReadFileToString("/proc/cmdline", &cmdline))
        return;

    const std::string key = ":qcom,mdss_dsi_";
    size_t pos = cmdline.find("mdss_mdp.panel=");
    if (pos == std::string::npos)
        return;
    pos = cmdline.find(key, pos);
    if (pos == std::string::npos)
        return;
    pos += key.size();

    std::string name = cmdline.substr(pos, cmdline.find_first_of(": \n", pos) - pos);
    std::string display = name;

    for (const auto &p : panels) {
        if (name.rfind(p.dsi_name, 0) == 0) {
            display = p.display;
            break;
        }
    }

    property_override("sys.panel.display", display);
}

void vendor_load_properties() {
    set_bootloader_prop();
    set_dalvik_heap();
    set_panel_prop();
}
