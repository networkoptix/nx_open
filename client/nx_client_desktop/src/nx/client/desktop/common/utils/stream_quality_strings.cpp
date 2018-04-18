#include "stream_quality_strings.h"

#include <QtCore/QCoreApplication>

namespace nx {
namespace client {
namespace desktop {

class StreamQualityStrings
{
    Q_DECLARE_TR_FUNCTIONS(StreamQualityStrings)

public:
    static QString displayString(Qn::StreamQuality value)
    {
        switch (value)
        {
            case Qn::QualityLowest:
                return tr("Lowest");
            case Qn::QualityLow:
                return tr("Low");
            case Qn::QualityNormal:
                return tr("Medium");
            case Qn::QualityHigh:
                return tr("High");
            case Qn::QualityHighest:
                return tr("Best");
            case Qn::QualityPreSet:
                return tr("Preset");
            case Qn::QualityNotDefined:
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
            case Qn::QualityLowest:
                //: Short for 'Lowest'
                return tr("Lst");
            case Qn::QualityLow:
                //: Short for 'Low'
                return tr("Lo");
            case Qn::QualityNormal:
                //: Short for 'Medium'
                return tr("Me");
            case Qn::QualityHigh:
                //: Short for 'High'
                return tr("Hi");
            case Qn::QualityHighest:
                //: Short for 'Best'
                return tr("Bst");
            case Qn::QualityPreSet:
                //: Short for 'Preset'
                return tr("Ps");
            case Qn::QualityNotDefined:
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

} // namespace desktop
} // namespace client
} // namespace nx
