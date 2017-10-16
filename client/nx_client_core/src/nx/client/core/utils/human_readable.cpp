#include "human_readable.h"

#include <text/time_strings.h>

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace core {

namespace {

// Previous (smaller) unit.
HumanReadable::TimeSpanUnit prevUnit(HumanReadable::TimeSpanUnit value)
{
    NX_EXPECT(value > HumanReadable::Milliseconds, Q_FUNC_INFO, "Mimimal unit already");
    return static_cast<HumanReadable::TimeSpanUnit>(value >> 1);
}

// Next (bigger) unit.
HumanReadable::TimeSpanUnit nextUnit(HumanReadable::TimeSpanUnit value)
{
    NX_EXPECT(value < HumanReadable::Years, Q_FUNC_INFO, "Maximal unit already");
    return static_cast<HumanReadable::TimeSpanUnit>(value << 1);
}

HumanReadable::TimeSpanUnit smallestUnit(HumanReadable::TimeSpanFormat format)
{
    if (format == HumanReadable::NoUnit)
        return HumanReadable::NoUnit;

    auto result = HumanReadable::Milliseconds;
    while (result < HumanReadable::Years && !format.testFlag(result))
        result = nextUnit(result);

    NX_EXPECT(format.testFlag(result), Q_FUNC_INFO, "Invalid format");
    return result;
};

//returns a string representation of time in a single time unit
QString unitString(
    const HumanReadable::TimeSpanUnit unit,
    const HumanReadable::SuffixFormat suffixFormat,
    const int count)
{
    auto suffixType = QnTimeStrings::Suffix::Milliseconds;

    switch (unit)
    {
        case HumanReadable::Seconds:
            suffixType = QnTimeStrings::Suffix::Seconds;
            break;
        case HumanReadable::Minutes:
            suffixType = QnTimeStrings::Suffix::Minutes;
            break;
        case HumanReadable::Hours:
            suffixType = QnTimeStrings::Suffix::Hours;
            break;
        case HumanReadable::Days:
            suffixType = QnTimeStrings::Suffix::Days;
            break;
        case HumanReadable::Weeks:
            suffixType = QnTimeStrings::Suffix::Weeks;
            break;
        case HumanReadable::Months:
            suffixType = QnTimeStrings::Suffix::Months;
            break;
        case HumanReadable::Years:
            suffixType = QnTimeStrings::Suffix::Years;
            break;
        default:
            break;
    }

    QString suffix;
    switch (suffixFormat)
    {
        case HumanReadable::SuffixFormat::Short:
            // Short suffix attached to number: 1m 15s
            suffix = QnTimeStrings::suffix(suffixType);
            break;
        case HumanReadable::SuffixFormat::Long:
            // Long suffix separated by space: 1 min 15 sec
            suffix = L' ' + QnTimeStrings::longSuffix(suffixType);
            break;
        case HumanReadable::SuffixFormat::Full:
            // Full suffix separated by space: 1 minute 15 seconds
            suffix = L' ' + QnTimeStrings::fullSuffix(suffixType, count);
            break;
        default:
            break;
    }

    return QString::number(count) + suffix;
}

template<class T>
struct PartDescriptor
{
    const T unit;
    const qint64 size;
    int count = 0;

    PartDescriptor(const T unit, const qint64 size):
        unit(unit),
        size(size)
    {
    }
};

using TimeSpanUnitDescriptor = PartDescriptor<HumanReadable::TimeSpanUnit>;

// Split given value by parts, descripted by units list. Returns true if at least one part was filled.
template<class T>
bool partition(qint64 value, std::vector<PartDescriptor<T>>& units)
{
    NX_EXPECT(!units.empty());
    NX_EXPECT(value >= 0);

    auto hasNonEmptyPart = false;
    for (auto& descriptor: units)
    {
        if (value < descriptor.size)
            continue;

        descriptor.count = value / descriptor.size;
        value = value % descriptor.size;

        NX_EXPECT(descriptor.count > 0);
        hasNonEmptyPart = true;
    }

    return hasNonEmptyPart;
}

std::vector<TimeSpanUnitDescriptor> timeSpanUnits(HumanReadable::TimeSpanFormat format)
{
    static constexpr auto kMillisecondSize = 1ll;
    static constexpr auto kSecondSize = kMillisecondSize * 1000;
    static constexpr auto kMinuteSize = kSecondSize * 60;
    static constexpr auto kHourSize = kMinuteSize * 60;
    static constexpr auto kDaySize = kHourSize * 24;
    static constexpr auto kWeekSize = kDaySize * 7;
    static constexpr auto kMonthSize = kDaySize * 31;
    static constexpr auto kYearSize = kDaySize * 366;

    std::vector<TimeSpanUnitDescriptor> result;

    if (format.testFlag(HumanReadable::Years))
        result.emplace_back(HumanReadable::Years, kYearSize);
    if (format.testFlag(HumanReadable::Months))
        result.emplace_back(HumanReadable::Months, kMonthSize);
    if (format.testFlag(HumanReadable::Weeks))
        result.emplace_back(HumanReadable::Weeks, kWeekSize);
    if (format.testFlag(HumanReadable::Days))
        result.emplace_back(HumanReadable::Days, kDaySize);
    if (format.testFlag(HumanReadable::Hours))
        result.emplace_back(HumanReadable::Hours, kHourSize);
    if (format.testFlag(HumanReadable::Minutes))
        result.emplace_back(HumanReadable::Minutes, kMinuteSize);
    if (format.testFlag(HumanReadable::Seconds))
        result.emplace_back(HumanReadable::Seconds, kSecondSize);
    if (format.testFlag(HumanReadable::Milliseconds))
        result.emplace_back(HumanReadable::Milliseconds, kMillisecondSize);

    return result;
}

} // namespace


QString HumanReadable::timeSpan(std::chrono::milliseconds ms,
    TimeSpanFormat format,
    SuffixFormat suffixFormat,
    const QString& separator,
    int suppressSecondUnitLimit)
{
    if (format == NoUnit)
        return QString();

    auto toUnitString =
            [suffixFormat](TimeSpanUnit unit, int num)
        {
            return unitString(unit, suffixFormat, num);
        };

    const auto smallest = smallestUnit(format);
    auto value = ms.count();
    const auto isNegative = value < 0;
    if (isNegative)
        value = -value;

    auto withSign = [isNegative](const QString& result)
        {
            return isNegative
                ? L'-' + result
                : result;
        };

    auto units = timeSpanUnits(format);

    // Check scenario where magnitude in milliseconds and format in seconds. Sign is not needed.
    if (!partition<TimeSpanUnit>(value, units))
        return toUnitString(smallest, 0);

    const auto primaryDescriptor = std::find_if(units.cbegin(), units.cend(),
        [](TimeSpanUnitDescriptor descriptor)
        {
            return descriptor.count > 0;
        });
    NX_EXPECT(primaryDescriptor != units.cend());
    if (primaryDescriptor == units.cend())
        return toUnitString(smallest, 0);

    auto secondaryDescriptor = primaryDescriptor + 1;
    while (secondaryDescriptor != units.cend() && !format.testFlag(secondaryDescriptor->unit))
        ++secondaryDescriptor;

    const bool showSecondary = secondaryDescriptor != units.cend()
        && secondaryDescriptor->count > 0
        && (suppressSecondUnitLimit == kNoSuppressSecondUnit
            || primaryDescriptor->count < suppressSecondUnitLimit);

    auto result = withSign(toUnitString(primaryDescriptor->unit, primaryDescriptor->count));

    if (showSecondary)
        result += separator + toUnitString(secondaryDescriptor->unit, secondaryDescriptor->count);

    return result;
}

} // namespace core
} // namespace client
} // namespace nx
