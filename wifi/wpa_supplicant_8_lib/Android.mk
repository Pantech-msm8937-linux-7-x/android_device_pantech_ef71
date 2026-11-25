#
# Copyright (C) 2026 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_8_X)

WPA_SUPPL_DIR := external/wpa_supplicant_8

include $(WPA_SUPPL_DIR)/wpa_supplicant/android.config

include $(CLEAR_VARS)
LOCAL_MODULE := lib_driver_cmd_ef71
LOCAL_LICENSE_KINDS := SPDX-license-identifier-BSD
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_VENDOR_MODULE := true
LOCAL_SHARED_LIBRARIES := libc libcutils liblog
LOCAL_HEADER_LIBRARIES := libcutils_headers
LOCAL_CFLAGS := -Wno-unused-parameter -DCONFIG_ANDROID_LOG -DCONFIG_DRIVER_NL80211
LOCAL_SRC_FILES := driver_cmd_nl80211.c
LOCAL_C_INCLUDES := \
    $(WPA_SUPPL_DIR)/src \
    $(WPA_SUPPL_DIR)/src/common \
    $(WPA_SUPPL_DIR)/src/drivers \
    $(WPA_SUPPL_DIR)/src/l2_packet \
    $(WPA_SUPPL_DIR)/src/utils \
    $(WPA_SUPPL_DIR)/src/wps \
    $(WPA_SUPPL_DIR)/wpa_supplicant \
    external/libnl/include
include $(BUILD_STATIC_LIBRARY)

endif
