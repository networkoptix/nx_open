#include "media_context.h"

#include <utils/media/av_codec_helper.h>

bool QnMediaContext::isSimilarTo(const QnConstMediaContextPtr& other) const
{
    if (getWidth() != other->getWidth() || getHeight() != other->getHeight())
        return false;

    // Comparing bits_per_coded_sample is needed for G726 audio codec.
    return getCodecId() == other->getCodecId() &&
        getBitsPerCodedSample() == other->getBitsPerCodedSample();
}

QString QnMediaContext::getCodecName() const 
{
    return QnAvCodecHelper::codecIdToString(getCodecId());
}

QString QnMediaContext::getAudioCodecDescription() const
{
    QString result;
    QString codecStr = QnAvCodecHelper::codecIdToString(getCodecId());
    if (!codecStr.isEmpty())
    {
        result += codecStr;
        result += QLatin1Char(' ');
    }

#if 0
    result += QString::number(c->getSampleRate() / 1000) + 
        QLatin1String("kHz ");
#endif

    if (getChannels() == 3)
        result += QLatin1String("2.1");
    else if (getChannels() == 6)
        result += QLatin1String("5.1");
    else if (getChannels() == 8)
        result += QLatin1String("7.1");
    else if (getChannels() == 2)
        result += QLatin1String("stereo");
    else if (getChannels() == 1)
        result += QLatin1String("mono");
    else
        result += QString::number(getChannels());

    return result;
}
