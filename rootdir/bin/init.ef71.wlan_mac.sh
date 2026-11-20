#! /vendor/bin/sh
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
# wcn36xx leaves the MAC address zero unless the bootloader passes one in
# the device tree, which the Pantech aboot does not. Derive a stable,
# locally administered address from the serial number instead, the way
# the Bluetooth address is derived.

for i in $(seq 1 60); do
    [ -e /sys/class/net/wlan0/address ] && break
    sleep 0.5
done

[ "$(cat /sys/class/net/wlan0/address)" = "00:00:00:00:00:00" ] || exit 0

serial=$(getprop ro.boot.serialno)
mac=$(printf '02%010s' "$serial" | tr ' ' '0' | cut -c1-12 | sed 's/\(..\)/\1:/g; s/:$//')

ifconfig wlan0 down
ifconfig wlan0 hw ether "$mac"
