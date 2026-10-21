// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp_stream_parser.h"

#include <nx/utils/log/log.h>

namespace nx::rtp {

void StreamParser::setFrequency(int frequency)
{
    if (frequency <= 0)
    {
        NX_WARNING(this, "Invalid frequency: %1", frequency);
        return;
    }

    m_frequency = frequency;
}

QnAbstractMediaDataPtr VideoStreamParser::nextData()
{
    if (!m_mediaData)
        return QnAbstractMediaDataPtr();

    QnAbstractMediaDataPtr result;
    result.swap(m_mediaData);
    if (!result->context)
    {
        // The parser has not been able to build codec parameters from the stream yet. Such a
        // frame can neither be described nor transcoded, so drop it instead of passing it on.
        NX_WARNING(this, "%1: Drop a video frame without codec parameters", logId());
        return QnAbstractMediaDataPtr();
    }
    return result;
}


QnAbstractMediaDataPtr AudioStreamParser::nextData()
{
    if (m_audioData.empty())
    {
        return QnAbstractMediaDataPtr();
    }
    else
    {
        auto result = m_audioData.front();
        m_audioData.pop_front();
        return result;
    }
}

} // namespace nx::rtp
