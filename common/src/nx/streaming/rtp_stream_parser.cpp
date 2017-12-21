#include "rtp_stream_parser.h"

namespace {

static const int kDefaultChunkContainerSize = 1024;

} // namespace

QnRtpVideoStreamParser::QnRtpVideoStreamParser()
{
    m_chunks.reserve(kDefaultChunkContainerSize);
}

void QnRtpStreamParser::setTimeHelper(QnRtspTimeHelper* timeHelper)
{
    m_timeHelper = timeHelper;
}

int QnRtpStreamParser::logicalChannelNum() const
{
    return m_logicalChannelNum;
}

void QnRtpStreamParser::setLogicalChannelNum(int value)
{
    m_logicalChannelNum = value;
}

void QnRtpAudioStreamParser::processIntParam(
    const QByteArray& checkName,
    int& setValue,
    const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;

    const auto paramName = param.left(valuePos);
    const auto paramValue = param.mid(valuePos+1);
    if (paramName.toLower() == checkName.toLower())
        setValue = paramValue.toInt();
}

void QnRtpAudioStreamParser::processHexParam(
    const QByteArray& checkName,
    QByteArray& setValue,
    const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;

    const auto paramName = param.left(valuePos);
    const auto paramValue = param.mid(valuePos+1);
    if (paramName.toLower() == checkName.toLower())
        setValue = QByteArray::fromHex(paramValue);
}

void QnRtpAudioStreamParser::processStringParam(
    const QByteArray& checkName,
    QByteArray& setValue,
    const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;

    const auto paramName = param.left(valuePos);
    const auto paramValue = param.mid(valuePos+1);
    if (paramName.toLower() == checkName.toLower())
        setValue = paramValue;
}

QnAbstractMediaDataPtr QnRtpVideoStreamParser::nextData()
{
    if (m_mediaData)
    {
        QnAbstractMediaDataPtr result;
        result.swap( m_mediaData );
        return result;
    }
    else
    {
        return QnAbstractMediaDataPtr();
    }
}

QnAbstractMediaDataPtr QnRtpAudioStreamParser::nextData()
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

int QnRtspAudioLayout::channelCount() const
{
    return 1;
}

QnResourceAudioLayout::AudioTrack QnRtspAudioLayout::getAudioTrackInfo(int /*index*/) const
{
    return m_audioTrack;
}

void QnRtspAudioLayout::setAudioTrackInfo(const AudioTrack& info)
{
    m_audioTrack = info;
}
