#include "stream_quality_strings.h"

#include <QtCore/QCoreApplication>

namespace nx::vms::client::desktop {

class StreamQualityStrings
{
    Q_DECLARE_TR_FUNCTIONS(StreamQualityStrings)

public:
    static QString displayString(Qn::StreamQuality value)
    {
        switch (value)
        {
            case Qn::StreamQuality::lowest:
                return tr("Lowest");
            case Qn::StreamQuality::low:
                return tr("Low");
            case Qn::StreamQuality::normal:
                return tr("Medium");
            case Qn::StreamQuality::high:
                return tr("High");
            case Qn::StreamQuality::highest:
                return tr("Best");
            case Qn::StreamQuality::preset:
                return tr("Preset");
            case Qn::StreamQuality::undefined:
                return tr("Undefined");
            default:
                return QString();
        }
    }

    static QString shortDisplayString(Qn::StreamQuality value)
    {
        /* Note that '//:' are comments for translators. */
        switch (value)
        {
            case Qn::StreamQuality::lowest:
                //: Short for 'Lowest'
                return tr("Lst");
            case Qn::StreamQuality::low:
                //: Short for 'Low'
                return tr("Lo");
            case Qn::StreamQuality::normal:
                //: Short for 'Medium'
                return tr("Me");
            case Qn::StreamQuality::high:
                //: Short for 'High'
                return tr("Hi");
            case Qn::StreamQuality::highest:
                //: Short for 'Best'
                return tr("Bst");
            case Qn::StreamQuality::preset:
                //: Short for 'Preset'
                return tr("Ps");
            case Qn::StreamQuality::undefined:
                return lit("-");
            default:

                return QString();
        }
    }
};

QString toDisplayString(Qn::StreamQuality value)
{
    return StreamQualityStrings::displayString(value);
}

QString toShortDisplayString(Qn::StreamQuality value)
{
    return StreamQualityStrings::shortDisplayString(value);
}

} // namespace nx::vms::client::desktop
