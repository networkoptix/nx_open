#pragma once

#include <chrono>

#include <analytics/common/object_detection_metadata.h>

namespace nx::analytics::db {

static constexpr bool kLookupObjectsInAnalyticsArchive = true;

// Using 14 bits for coordinates. That allows using only 2 bytes when compressing the value.
static constexpr int kCoordinatesBits = 14;
static constexpr int kCoordinatesPrecision = (1 << kCoordinatesBits) - 1;
static constexpr int kCoordinateDecimalDigits = nx::common::metadata::kCoordinateDecimalDigits;
// NOTE: If the assertion fails, all constants here must be modified and analytics DB data migrated.
static_assert(kCoordinateDecimalDigits == 4);

static constexpr int kMaxObjectLookupResultSet = 1000;

// NOTE: Limiting filtered_events subquery to make query
// CPU/memory requirements much less dependent of DB size.
// Assuming that objects tracks are whether interleaved or quite short.
// So, in situation, when there is a single 100,000 - records long object track
// selected by filter less objects than requested filter.maxObjectsToSelect would be returned.
static constexpr int kMaxExpectedTrackLength = 100;
static constexpr int kMaxFilterEventsResultSize =
    kMaxExpectedTrackLength * kMaxObjectLookupResultSet;

/**
 * Time periods shorter than this value are always aggregated.
 */
static constexpr auto kMinTimePeriodAggregationPeriod = std::chrono::seconds(3);

static constexpr int kUsecInMs = 1000;

static constexpr auto kTrackAggregationPeriod = std::chrono::seconds(5);
static constexpr auto kMaxCachedObjectLifeTime = std::chrono::minutes(1);
static constexpr auto kTrackSearchResolutionX = 44;
static constexpr auto kTrackSearchResolutionY = 32;

} // namespace nx::analytics::db
