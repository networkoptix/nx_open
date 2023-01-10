// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_quality_strings.h"

#include <QtCore/QCoreApplication>

namespace nx::vms::client::desktop {

using StreamQuality = nx::vms::api::StreamQuality;

class StreamQualityStrings
{
    Q_DECLARE_TR_FUNCTIONS(StreamQualityStrings)

public:
    static QString displayString(nx::vms::api::StreamQuality value)
    {
        switch (value)
        {
            case StreamQuality::lowest:
                return tr("Lowest");
            case StreamQuality::low:
                return tr("Low");
            case StreamQuality::normal:
                return tr("Medium");
            case StreamQuality::high:
                return tr("High");
            case StreamQuality::highest:
                return tr("Best");
            case StreamQuality::preset:
                return tr("Preset");
            case StreamQuality::undefined:
                return tr("Undefined");

            default:
                return QString();
        }
    }

    static QString shortDisplayString(StreamQuality value)
    {
        // Note that '//:' are comments for translators.
        switch (value)
        {
            case StreamQuality::lowest:
                //: Short for 'Lowest'
                return tr("Lst");
            case StreamQuality::low:
                //: Short for 'Low'
                return tr("Lo");
            case StreamQuality::normal:
                //: Short for 'Medium'
                return tr("Me");
            case StreamQuality::high:
                //: Short for 'High'
                return tr("Hi");
            case StreamQuality::highest:
                //: Short for 'Best'
                return tr("Bst");
            case StreamQuality::preset:
                //: Short for 'Preset'
                return tr("Ps");
            case StreamQuality::undefined:
                return "-";

            default:
                return QString();
        }
    }
};

QString toDisplayString(StreamQuality value)
{
    return StreamQualityStrings::displayString(value);
}

QString toShortDisplayString(StreamQuality value)
{
    return StreamQualityStrings::shortDisplayString(value);
}

} // namespace nx::vms::client::desktop
