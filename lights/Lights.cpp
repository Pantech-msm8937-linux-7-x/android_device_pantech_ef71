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

#define LOG_TAG "lights-ef71"

#include "Lights.h"

#include <algorithm>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

using ::android::base::ParseInt;
using ::android::base::ReadFileToString;
using ::android::base::Trim;
using ::android::base::WriteStringToFile;

namespace aidl {
namespace android {
namespace hardware {
namespace light {

namespace {

constexpr const char* kLcdBrightness = "/sys/class/leds/lcd-backlight/brightness";
constexpr const char* kLcdMaxBrightness = "/sys/class/leds/lcd-backlight/max_brightness";

/*
 * The EF71 has one LP5523 for the front indicator (a left and a right RGB
 * LED) and the menu and back key backlights. Pantech's driver, as on the
 * stock kernel, takes the channel brightnesses as they are written and
 * applies them as the command written to channel 8 says
 * (PAN_LED_LP5523_BLINK_CMD), as the stock lights HAL wrote them.
 */
enum Lp5523Channel {
    kLeftGreen = 0,
    kLeftBlue = 1,
    kRightGreen = 2,
    kRightBlue = 3,
    kMenuKey = 4,
    kBackKey = 5,
    kLeftRed = 6,
    kRightRed = 7,
    kCommand = 8,
};

enum Lp5523Command {
    kCmdOff = 0,             // EF59_LED_OFF: the indicator off
    kCmdKeys = 1,            // PWM_TK_ON_OFF: apply the key backlights
    kCmdSolid = 3,           // PWM_HOMEKEY_ON_OFF: the color, steady
    kCmdBreathe = 4,         // DIMMING_HOMEKEY_ON_OFF: the color, dimming in and out
    kCmdBlinkTimer = 10,     // DIMMING_ON_COLOR_TIMER: blink, off time in 484 ms
    kCmdBlinkShortTimer = 11,  // DIMMING_ON_OFF_SHORT_TIMER: blink, off time in 15 ms
    kCmdGreen2mA = 102,      // EF59_LED_SET_GREEN_LIGHT_VALUE_20
    kCmdGreen4mA = 103,      // EF59_LED_SET_GREEN_LIGHT_VALUE_40
    kCmdRedCharging = 200,   // DIMMING_RED_CHARGING: red dimming while charging
};

// The blink timings, in the right green and blue channels (at most 61)
constexpr int kMaxTiming = 61;
constexpr uint32_t kDarkGreen = 0x80;

std::string channelPath(int channel) {
    return "/sys/class/leds/lp5523:channel" + std::to_string(channel) + "/brightness";
}

bool writeInt(const std::string& path, int value) {
    if (!WriteStringToFile(std::to_string(value), path)) {
        PLOG(ERROR) << "Failed to write " << value << " to " << path;
        return false;
    }
    return true;
}

void writeChannel(int channel, int value) {
    writeInt(channelPath(channel), value);
}

bool isLit(const HwLightState& state) {
    return state.color & 0x00ffffff;
}

// Brightness of an ARGB color, the alpha scaling the color as the framework intends
uint32_t argbToBrightness(uint32_t color) {
    uint32_t alpha = (color >> 24) & 0xff;
    uint32_t red = (color >> 16) & 0xff;
    uint32_t green = (color >> 8) & 0xff;
    uint32_t blue = color & 0xff;

    if (alpha != 0xff) {
        red = red * alpha / 0xff;
        green = green * alpha / 0xff;
        blue = blue * alpha / 0xff;
    }
    return (77 * red + 150 * green + 29 * blue) >> 8;
}

#define LIGHT(t) {.id = static_cast<int>(t), .ordinal = 0, .type = t}

const std::vector<HwLight> kLights = {
        LIGHT(LightType::BACKLIGHT),
        LIGHT(LightType::BUTTONS),
        LIGHT(LightType::BATTERY),
        LIGHT(LightType::NOTIFICATIONS),
        LIGHT(LightType::ATTENTION),
};

}  // namespace

Lights::Lights() : mLcdMaxBrightness(255) {
    std::string value;
    int max;

    if (ReadFileToString(kLcdMaxBrightness, &value) && ParseInt(Trim(value), &max) && max > 0) {
        mLcdMaxBrightness = max;
    }
    LOG(INFO) << "LCD max brightness " << mLcdMaxBrightness;
}

ndk::ScopedAStatus Lights::setLightState(int id, const HwLightState& state) {
    switch (static_cast<LightType>(id)) {
        case LightType::BACKLIGHT:
            setBacklight(state);
            break;
        case LightType::BUTTONS:
            setButtons(state);
            break;
        case LightType::BATTERY: {
            std::lock_guard<std::mutex> lock(mLock);
            mBattery = state;
            updateIndicatorLocked();
            break;
        }
        case LightType::NOTIFICATIONS: {
            std::lock_guard<std::mutex> lock(mLock);
            mNotification = state;
            updateIndicatorLocked();
            break;
        }
        case LightType::ATTENTION: {
            std::lock_guard<std::mutex> lock(mLock);
            mAttention = state;
            // PowerManager turns the attention light off with an empty flash
            if (mAttention.flashOnMs == 0 && mAttention.flashOffMs == 0) {
                mAttention.color = 0;
            }
            updateIndicatorLocked();
            break;
        }
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* lights) {
    *lights = kLights;
    return ndk::ScopedAStatus::ok();
}

void Lights::setBacklight(const HwLightState& state) {
    writeInt(kLcdBrightness, argbToBrightness(state.color) * mLcdMaxBrightness / 0xff);
}

void Lights::setButtons(const HwLightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    int brightness = argbToBrightness(state.color);

    writeChannel(kMenuKey, brightness);
    writeChannel(kBackKey, brightness);
    writeChannel(kCommand, kCmdKeys);
}

void Lights::updateIndicatorLocked() {
    if (isLit(mAttention)) {
        setIndicatorLocked(mAttention, false);
    } else if (isLit(mNotification)) {
        setIndicatorLocked(mNotification, false);
    } else {
        setIndicatorLocked(mBattery, true);
    }
}

// The current of the green LEDs: 2 mA for the dark green of a full battery,
// as the stock HAL had it, 4 mA otherwise
void Lights::setGreenCurrentLocked(uint32_t green) {
    int cmd = green == kDarkGreen ? kCmdGreen2mA : kCmdGreen4mA;

    if (cmd != mGreenCurrentCmd) {
        writeChannel(kCommand, cmd);
        mGreenCurrentCmd = cmd;
    }
}

void Lights::setIndicatorLocked(const HwLightState& state, bool battery) {
    uint32_t alpha = (state.color >> 24) & 0xff;
    uint32_t red = (state.color >> 16) & 0xff;
    uint32_t green = (state.color >> 8) & 0xff;
    uint32_t blue = state.color & 0xff;

    if (alpha != 0xff) {
        red = red * alpha / 0xff;
        green = green * alpha / 0xff;
        blue = blue * alpha / 0xff;
    }

    if (!red && !green && !blue) {
        writeChannel(kCommand, kCmdOff);
        return;
    }

    setGreenCurrentLocked(green);

    bool blink = (state.flashMode == FlashMode::TIMED || state.flashMode == FlashMode::HARDWARE) &&
                 state.flashOnMs > 0 && state.flashOffMs > 0;

    if (blink) {
        // The color on the left channels, the timings on the right ones
        int cmd, off;

        if (state.flashOffMs >= 485) {
            cmd = kCmdBlinkTimer;
            off = state.flashOffMs / 484;
        } else {
            cmd = kCmdBlinkShortTimer;
            off = state.flashOffMs / 15;
        }
        writeChannel(kLeftRed, red);
        writeChannel(kLeftGreen, green);
        writeChannel(kLeftBlue, blue);
        writeChannel(kRightRed, 0);
        writeChannel(kRightGreen, std::min(state.flashOnMs / 15, kMaxTiming));
        writeChannel(kRightBlue, std::min(off, kMaxTiming));
        writeChannel(kCommand, cmd);
        return;
    }

    // While charging, the stock indicator dims red in and out; full, it is green
    if (battery && red && !(green > red)) {
        writeChannel(kCommand, kCmdRedCharging);
        return;
    }

    writeChannel(kLeftRed, red);
    writeChannel(kLeftGreen, green);
    writeChannel(kLeftBlue, blue);
    writeChannel(kRightRed, red);
    writeChannel(kRightGreen, green);
    writeChannel(kRightBlue, blue);
    // The attention light (incoming calls) dims in and out, as on the stock
    writeChannel(kCommand, &state == &mAttention ? kCmdBreathe : kCmdSolid);
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
