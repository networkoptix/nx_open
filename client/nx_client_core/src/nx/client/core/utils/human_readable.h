#pragma once

#include <chrono>

#include <QtCore/QFlags>
#include <QtCore/QString>

namespace nx {
namespace client {
namespace core {

class HumanReadable
{
public:
    enum TimeSpanUnit
    {
        NoUnit = 0,
        Milliseconds = 0x0001,
        Seconds = 0x0002,
        Minutes = 0x0004,
        Hours = 0x0008,
        Days = 0x0010,
        Weeks = 0x0020,
        Months = 0x0040,
        Years = 0x0080,
        DaysAndTime = Days | Hours | Minutes | Seconds,
        AllUnits = Milliseconds | DaysAndTime | Months | Years,
    };
    Q_DECLARE_FLAGS(TimeSpanFormat, TimeSpanUnit);

    enum class SuffixFormat
    {
        Short,
        Long,
        Full
    };

    static const int kNoSuppressSecondUnit = -1;
    static const int kDefaultSuppressSecondUnitLimit = 3;

    static QString timeSpan(std::chrono::milliseconds ms,
        TimeSpanFormat format = DaysAndTime,
        SuffixFormat suffixFormat = SuffixFormat::Full,
        const QString& separator = QLatin1String(", "),
        int suppressSecondUnitLimit = kDefaultSuppressSecondUnitLimit);

};

Q_DECLARE_OPERATORS_FOR_FLAGS(HumanReadable::TimeSpanFormat);


} // namespace core
} // namespace client
} // namespace nx
