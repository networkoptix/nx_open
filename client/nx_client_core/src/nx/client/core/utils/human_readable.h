#pragma once

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

namespace nx {
namespace client {
namespace core {

class HumanReadable
{
    Q_GADGET
    Q_DECLARE_TR_FUNCTIONS(HumanReadable)

public:
    enum TimeSpanUnit
    {
        Milliseconds = 0x0001,
        Seconds = 0x0002,
        Minutes = 0x0004,
        Hours = 0x0008,
        Days = 0x0010,
        Weeks = 0x0020,
        Months = 0x0040,
        Years = 0x0080,
        DaysAndTime = Days | Hours | Minutes | Seconds,
        AllTimeUnits = Milliseconds | DaysAndTime | Months | Years,

        MaxTimeSpanUnit = Years
    };
    Q_ENUM(TimeSpanUnit)
    Q_DECLARE_FLAGS(TimeSpanFormat, TimeSpanUnit);

    enum DigitalSizeUnit
    {
        Bytes = 0x0001,
        Kilo = 0x0002,
        Mega = 0x0004,
        Giga = 0x0008,
        Tera = 0x0010,
        Peta = 0x0020,
        FileSize = Bytes | Kilo | Mega | Giga,
        VolumeSize = Giga | Tera | Peta,
        AllDigitalSizeUnits = FileSize | VolumeSize,

        MaxDigitalSizeUnit = Peta
    };
    Q_ENUM(DigitalSizeUnit)
    Q_DECLARE_FLAGS(DigitalSizeFormat, DigitalSizeUnit);

    enum class SuffixFormat
    {
        Short,
        Long,
        Full
    };

    enum class DigitalSizeMultiplier
    {
        Decimal,
        Binary
    };

    static const int kNoSuppressSecondUnit = -1;
    static const int kDefaultSuppressSecondUnitLimit = 3;

    static QString timeSpan(std::chrono::milliseconds ms,
        TimeSpanFormat format = DaysAndTime,
        SuffixFormat suffixFormat = SuffixFormat::Full,
        const QString& separator = QLatin1String(", "),
        int suppressSecondUnitLimit = kDefaultSuppressSecondUnitLimit);

    /**
     * Suffix for the given unit. All suffixes are using decimal standard.
     **/
    static QString digitalSizeUnit(DigitalSizeUnit unit,
        SuffixFormat suffixFormat = SuffixFormat::Short,
        int count = -1);

    static QString digitalSize(qint64 size,
        DigitalSizeFormat format = FileSize,
        DigitalSizeMultiplier multiplier = DigitalSizeMultiplier::Binary,
        SuffixFormat suffixFormat = SuffixFormat::Short,
        const QString& separator = QLatin1String(" "),
        int suppressSecondUnitLimit = 200);

};

Q_DECLARE_OPERATORS_FOR_FLAGS(HumanReadable::TimeSpanFormat);


} // namespace core
} // namespace client
} // namespace nx
