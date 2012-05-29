#include "rtp_stream_parser.h"

QnRtpStreamParser::QnRtpStreamParser()
{
}

QnRtpStreamParser::~QnRtpStreamParser()
{
}

void QnRtpAudioStreamParser::processIntParam(const QByteArray& checkName, int& setValue, const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName == checkName)
        setValue = paramValue.toInt();
}

void QnRtpAudioStreamParser::processHexParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName == checkName)
        setValue = QByteArray::fromHex(paramValue);
}

void QnRtpAudioStreamParser::processStringParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName == checkName)
        setValue = paramValue;
}

