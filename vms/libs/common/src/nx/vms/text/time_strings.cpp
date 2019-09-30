#include "time_strings.h"

QString QnTimeStrings::suffix(Suffix value)
{
    switch (value)
    {
        case Suffix::Milliseconds:
            return tr("ms", "Suffix for displaying milliseconds");
        case Suffix::Seconds:
            return tr("s", "Suffix for displaying seconds");
        case Suffix::Minutes:
            return tr("m", "Suffix for displaying minutes");
        case Suffix::Hours:
            return tr("h", "Suffix for displaying hours");
        case Suffix::Days:
            return tr("d", "Suffix for displaying days");
        case Suffix::Weeks:
            return tr("w", "Suffix for displaying weeks");
        case Suffix::Months:
            return tr("M", "Suffix for displaying months");
        case Suffix::Years:
            return tr("y", "Suffix for displaying years");
    }

    return QString();
}

QString QnTimeStrings::longSuffix(Suffix value)
{
    switch (value)
    {
        case Suffix::Milliseconds:
            return tr("msec", "Long suffix for displaying milliseconds");
        case Suffix::Seconds:
            return tr("sec", "Long suffix for displaying seconds");
        case Suffix::Minutes:
            return tr("min", "Long suffix for displaying minutes");
        case Suffix::Hours:
            return tr("hrs", "Long suffix for displaying hours");
        case Suffix::Days:
            return tr("days", "Long suffix for displaying days");
        case Suffix::Weeks:
            return tr("wks", "Long suffix for displaying weeks");
        case Suffix::Months:
            return tr("mos", "Long suffix for displaying months");
        case Suffix::Years:
            return tr("yrs", "Long suffix for displaying years");
    }

    return QString();
}

QString QnTimeStrings::fullSuffix(Suffix value, int count)
{
    switch (value)
    {
        case Suffix::Milliseconds:
            return tr("milliseconds", "Full suffix for displaying milliseconds", count);
        case Suffix::Seconds:
            return tr("seconds", "Full suffix for displaying seconds", count);
        case Suffix::Minutes:
            return tr("minutes", "Full suffix for displaying minutes", count);
        case Suffix::Hours:
            return tr("hours", "Full suffix for displaying hours", count);
        case Suffix::Days:
            return tr("days", "Full suffix for displaying days", count);
        case Suffix::Weeks:
            return tr("weeks", "Full suffix for displaying weeks", count);
        case Suffix::Months:
            return tr("months", "Full suffix for displaying months", count);
        case Suffix::Years:
            return tr("years", "Full suffix for displaying years", count);
    }

    return QString();
}
