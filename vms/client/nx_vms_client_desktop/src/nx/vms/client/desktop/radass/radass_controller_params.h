// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

/**
 * These parameters control operation of RadAss controller.
 * They are used in RadAss controller itself and its tests.
 */

namespace nx::vms::client::desktop::radass {

using namespace std::literals::chrono_literals;

// Time to ignore recently added cameras.
static constexpr auto kRecentlyAddedInterval = 1s;

// Delay between quality switching attempts.
static constexpr auto kQualitySwitchInterval = 5s;

// Delay between checks if performance can be improved.
static constexpr auto kPerformanceLossCheckInterval = 5s;

// Put item to LQ if visual size is small.
static const QSize kLowQualityScreenSize(320.0 / 1.4, 240.0 / 1.4);

// Put item back to HQ if visual size increased to kLowQualityScreenSize * kLqToHqSizeThreshold.
static constexpr double kLqToHqSizeThreshold = 1.34;

// Try to balance every 500ms
static constexpr auto kTimerInterval = 500ms;

// Every 10 minutes try to raise quality. Value in counter ticks.
static constexpr int kAdditionalHQRetryCounter = 10 * 60 * 1000
    / static_cast<std::chrono::milliseconds>(kTimerInterval).count();

// Ignore cameras which are on Fast Forward
static constexpr double kSpeedValueEpsilon = 0.0001;

// Switch all cameras to LQ if there are more than 16 cameras on the scene.
static constexpr int kMaximumConsumersCount = 16;

// Network considered as slow if there are less than 3 frames in display queue.
static constexpr int kSlowNetworkFrameLimit = 3;

// Item will go to LQ if it is small for this period of time already.
static constexpr auto kLowerSmallItemQualityInterval = 1s;

static constexpr int kAutomaticSpeed = std::numeric_limits<int>::max();

} // namespace nx::vms::client::desktop::radass
