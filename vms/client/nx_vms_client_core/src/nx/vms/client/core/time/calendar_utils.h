// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QBitArray>
#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include <recording/time_period_list.h>

namespace nx::vms::client::core::calendar_utils {

constexpr int kMinYear = 1970;

constexpr int kMinMonth = 1;
constexpr int kMaxMonth = 12;

// As we directly manipulate with display offsets we have increased values and range despite the
// standard QTimeZone values.
const int kOffsetRange = std::abs(QTimeZone::MinUtcOffsetSecs)
    + QTimeZone::MaxUtcOffsetSecs;
const int kMaxDisplayOffset = std::chrono::milliseconds(
    std::chrono::seconds(kOffsetRange)).count();
const int kMinDisplayOffset = -kMaxDisplayOffset;

NX_VMS_CLIENT_CORE_API QDate firstWeekStartDate(const QLocale& locale, int year, int month);

NX_VMS_CLIENT_CORE_API QBitArray buildArchivePresence(
    const QnTimePeriodList& timePeriods,
    const std::chrono::milliseconds& startTimestamp,
    int count,
    const std::function<std::chrono::milliseconds(std::chrono::milliseconds)>& nextTimestamp);

} // namespace nx::vms::client::core::calendar_utils
