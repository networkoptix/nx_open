#include "human_readable.h"

#include <QtCore/QMetaEnum>

#include <text/time_strings.h>

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace core {

namespace {

template<typename T>
T minUnit()
{
    auto metaEnum = QMetaEnum::fromType<T>();
    const auto count = metaEnum.keyCount();
    return static_cast<T>(metaEnum.value(0));
}

template<typename T>
T maxUnit()
{
    auto metaEnum = QMetaEnum::fromType<T>();
    const auto count = metaEnum.keyCount();
    return static_cast<T>(metaEnum.value(count - 1));
}

// Previous (smaller) unit.
template<typename T>
T prevUnit(T value)
{
    NX_EXPECT(value > minUnit<T>(), Q_FUNC_INFO, "Mimimal unit already");
    return static_cast<T>(value >> 1);
}

// Next (bigger) unit.
template<typename T>
T nextUnit(T value)
{
    NX_EXPECT(value < maxUnit<T>(), Q_FUNC_INFO, "Maximal unit already");
    return static_cast<T>(value << 1);
}

template<typename T>
T smallestUnit(QFlags<T> format)
{
    auto result = minUnit<T>();
    while (result < maxUnit<T>() && !format.testFlag(result))
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

template<typename T>
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

// Split given value by parts, descripted by units list. Returns true if at least one part was filled.
template<typename T>
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


using TimeSpanUnitDescriptor = PartDescriptor<HumanReadable::TimeSpanUnit>;

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

using DigitalSizeUnitDescriptor = PartDescriptor<HumanReadable::DigitalSizeUnit>;
std::vector<DigitalSizeUnitDescriptor> digitalSizeUnits(HumanReadable::DigitalSizeFormat format,
    const HumanReadable::DigitalSizeMultiplier multiplier)
{
    static constexpr auto kByteSize = 1ll;

    static constexpr auto kBinaryMultiplier = 1024;
    static constexpr auto kKibiSize = kByteSize * kBinaryMultiplier;
    static constexpr auto kMebiSize = kKibiSize * kBinaryMultiplier;
    static constexpr auto kGibiSize = kMebiSize * kBinaryMultiplier;
    static constexpr auto kTebiSize = kGibiSize * kBinaryMultiplier;
    static constexpr auto kPebiSize = kTebiSize * kBinaryMultiplier;

    static constexpr auto kDecimalMultiplier = 1000;
    static constexpr auto kKiloSize = kByteSize * kDecimalMultiplier;
    static constexpr auto kMegaSize = kKiloSize * kDecimalMultiplier;
    static constexpr auto kGigaSize = kMegaSize * kDecimalMultiplier;
    static constexpr auto kTeraSize = kGigaSize * kDecimalMultiplier;
    static constexpr auto kPetaSize = kTeraSize * kDecimalMultiplier;

    const auto isBinary = multiplier == HumanReadable::DigitalSizeMultiplier::Binary;

    std::vector<DigitalSizeUnitDescriptor> result;
    if (format.testFlag(HumanReadable::Peta))
        result.emplace_back(HumanReadable::Peta, isBinary ? kPebiSize : kPetaSize);
    if (format.testFlag(HumanReadable::Tera))
        result.emplace_back(HumanReadable::Tera, isBinary ? kTebiSize : kTeraSize);
    if (format.testFlag(HumanReadable::Giga))
        result.emplace_back(HumanReadable::Giga, isBinary ? kGibiSize : kGigaSize);
    if (format.testFlag(HumanReadable::Mega))
        result.emplace_back(HumanReadable::Mega, isBinary ? kMebiSize : kMegaSize);
    if (format.testFlag(HumanReadable::Kilo))
        result.emplace_back(HumanReadable::Kilo, isBinary ? kKibiSize : kKiloSize);
    if (format.testFlag(HumanReadable::Bytes))
        result.emplace_back(HumanReadable::Bytes, kByteSize);

    return result;
}

template<typename T>
QString calculateValueInternal(qint64 sourceValue,
    QFlags<T> format,
    std::function<QString(T unit, int num)> toUnitString,
    std::vector<PartDescriptor<T>> units,
    const QString& separator,
    int suppressSecondUnitLimit)
{
    const auto smallest = smallestUnit(format);
    auto value = sourceValue;
    const auto isNegative = value < 0;
    if (isNegative)
        value = -value;

    auto withSign =
        [isNegative](const QString& result)
        {
            return isNegative
                ? L'-' + result
                : result;
        };

    // Check scenario where magnitude in milliseconds and format in seconds. Sign is not needed.
    if (!partition(value, units))
        return toUnitString(smallest, 0);

    const auto primaryDescriptor = std::find_if(units.cbegin(), units.cend(),
        [](PartDescriptor<T> descriptor)
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
        && (suppressSecondUnitLimit == HumanReadable::kNoSuppressSecondUnit
            || primaryDescriptor->count < suppressSecondUnitLimit);

    auto result = withSign(toUnitString(primaryDescriptor->unit, primaryDescriptor->count));

    if (showSecondary)
        result += separator + toUnitString(secondaryDescriptor->unit, secondaryDescriptor->count);

    return result;
}

} // namespace


QString HumanReadable::timeSpan(std::chrono::milliseconds ms,
    TimeSpanFormat format,
    SuffixFormat suffixFormat,
    const QString& separator,
    int suppressSecondUnitLimit)
{
    auto toUnitString =
        [suffixFormat](TimeSpanUnit unit, int num)
        {
            return unitString(unit, suffixFormat, num);
        };
    const auto units = timeSpanUnits(format);

    return calculateValueInternal<TimeSpanUnit>(ms.count(),
        format,
        toUnitString,
        units,
        separator,
        suppressSecondUnitLimit);
}

QString HumanReadable::digitalSizeUnit(DigitalSizeUnit unit, SuffixFormat suffixFormat, int count)
{
    switch (unit)
    {
        case Bytes:
        {
            return (suffixFormat == SuffixFormat::Full)
                ? tr("Bytes", "Full suffix for displaying bytes", count)
                : tr("B", "Suffix for displaying bytes", count);
        }
        case Kilo:
        {
            return (suffixFormat == SuffixFormat::Full)
                ? tr("Kilobytes", "Full suffix for displaying kilobytes", count)
                : tr("kB", "Suffix for displaying kilobytes", count);
        }
        case Mega:
        {
            return (suffixFormat == SuffixFormat::Full)
                ? tr("Megabytes", "Full suffix for displaying megabytes", count)
                : tr("MB", "Suffix for displaying megabytes", count);
        }
        case Giga:
        {
            return (suffixFormat == SuffixFormat::Full)
                ? tr("Gigabytes", "Full suffix for displaying gigabytes", count)
                : tr("GB", "Suffix for displaying gigabytes", count);
        }
        case Tera:
        {
            return (suffixFormat == SuffixFormat::Full)
                ? tr("Terabytes", "Full suffix for displaying terabytes", count)
                : tr("TB", "Suffix for displaying terabytes", count);
        }
        case Peta:
        {
            return (suffixFormat == SuffixFormat::Full)
                ? tr("Petabytes", "Full suffix for displaying petabytes", count)
                : tr("PB", "Suffix for displaying petabytes", count);
        }
        default:
            break;
    }
    return QString();
}

QString HumanReadable::digitalSize(qint64 size,
    DigitalSizeFormat format,
    DigitalSizeMultiplier multiplier,
    SuffixFormat suffixFormat,
    const QString& separator,
    int suppressSecondUnitLimit)
{
    auto toUnitString =
        [suffixFormat](DigitalSizeUnit unit, int num)
        {
            QString suffix = digitalSizeUnit(unit, suffixFormat, num);
            switch (suffixFormat)
            {
                case SuffixFormat::Long:
                case SuffixFormat::Full:
                    suffix = L' ' + suffix;
                    break;
                default:
                    break;
            }

            return QString::number(num) + suffix;
        };
    const auto units = digitalSizeUnits(format, multiplier);

    return calculateValueInternal<DigitalSizeUnit>(size,
        format,
        toUnitString,
        units,
        separator,
        suppressSecondUnitLimit);
}

} // namespace core
} // namespace client
} // namespace nx
