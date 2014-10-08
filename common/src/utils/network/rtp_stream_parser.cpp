#include "rtp_stream_parser.h"


QnRtpVideoStreamParser::QnRtpVideoStreamParser()
{
    m_chunks.reserve(1024);
}

QnRtpStreamParser::QnRtpStreamParser():
    m_timeHelper(0),
    m_logicalChannelNum(0)
{
}

QnRtpStreamParser::~QnRtpStreamParser()
{
}

void QnRtpStreamParser::setTimeHelper(QnRtspTimeHelper* timeHelper)
{
    m_timeHelper = timeHelper;
}

void QnRtpAudioStreamParser::processIntParam(const QByteArray& checkName, int& setValue, const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName.toLower() == checkName.toLower())
        setValue = paramValue.toInt();
}

void QnRtpAudioStreamParser::processHexParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName.toLower() == checkName.toLower())
        setValue = QByteArray::fromHex(paramValue);
}

void QnRtpAudioStreamParser::processStringParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName.toLower() == checkName.toLower())
        setValue = paramValue;
}

QnAbstractMediaDataPtr QnRtpVideoStreamParser::nextData()
{
    if (m_mediaData) {
        QnAbstractMediaDataPtr result = m_mediaData;
        m_mediaData.clear();
        return result;
    }
    else {
        return QnAbstractMediaDataPtr();
    }
}

QnAbstractMediaDataPtr QnRtpAudioStreamParser::nextData()
{
    if (m_audioData.empty())
        return QnAbstractMediaDataPtr();
    else {
        QnAbstractMediaDataPtr result = m_audioData.front();
        m_audioData.pop_front();
        return result;
    }
}
