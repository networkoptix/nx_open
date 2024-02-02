// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp_stream_parser.h"

namespace nx::rtp {

QnAbstractMediaDataPtr VideoStreamParser::nextData()
{
    if (m_mediaData)
    {
        QnAbstractMediaDataPtr result;
        result.swap(m_mediaData);
        return result;
    }
    else
    {
        return QnAbstractMediaDataPtr();
    }
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
