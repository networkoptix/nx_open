#include "time_constants.h"

#include <limits>
#include <QtCore/QDateTime>

namespace {

static constexpr int kMaxOffset =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(14)).count();

} // namespace


namespace nx::client::core {

const int TimeConstants::kMinYear = QDateTime::fromMSecsSinceEpoch(0, Qt::UTC).date().year();
const int TimeConstants::kMaxYear = QDateTime::fromMSecsSinceEpoch(
    std::numeric_limits<qint64>::max(), Qt::UTC).date().year();

const int TimeConstants::kMinMonth = 1;
const int TimeConstants::kMaxMonth = 12;

// According to the Qt documentation time zones offset range is in [-14..14] hours range.
const int TimeConstants::kMinDisplayOffset = -kMaxOffset;
const int TimeConstants::kMaxDisplayOffset = kMaxOffset;

} // namespace nx::client::core
