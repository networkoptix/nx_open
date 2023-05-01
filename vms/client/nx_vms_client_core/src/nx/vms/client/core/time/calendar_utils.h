// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QBitArray>
#include <QtCore/QDate>
#include <QtCore/QLocale>

#include <recording/time_period_list.h>

namespace nx::vms::client::core::calendar_utils {

constexpr int kMinYear = 1970;

constexpr int kMinMonth = 1;
constexpr int kMaxMonth = 12;

// According to the Qt documentation time zones offset range is in [-14..14] hours range.
constexpr int kMaxDisplayOffset = std::chrono::milliseconds(std::chrono::hours(14)).count();
constexpr int kMinDisplayOffset = -kMaxDisplayOffset;

NX_VMS_CLIENT_CORE_API QDate firstWeekStartDate(const QLocale& locale, int year, int month);

NX_VMS_CLIENT_CORE_API QBitArray buildArchivePresence(
    const QnTimePeriodList& timePeriods,
    const std::chrono::milliseconds& startTimestamp,
    int count,
    const std::function<std::chrono::milliseconds(std::chrono::milliseconds)>& nextTimestamp);

} // namespace nx::vms::client::core::calendar_utils
