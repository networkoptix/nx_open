#include "sdp.h"

namespace nx::streaming {

static Sdp::MediaType mediaTypeFromString(const QString& value)
{
    QString trackTypeStr = value.toLower();
    if (trackTypeStr == "audio")
        return Sdp::MediaType::Audio;
    else if (trackTypeStr == "video")
        return Sdp::MediaType::Video;
    else if (trackTypeStr == "metadata")
        return Sdp::MediaType::Metadata;
    else
        return Sdp::MediaType::Unknown;
}

// see rfc1890 for full RTP predefined codec list
static QString findCodecById(int num)
{
    switch (num)
    {
        case 0: return QString("PCMU");
        case 8: return QString("PCMA");
        case 26: return QString("JPEG");
        default: return QString();
    }
}

bool parseRtpMap(const QString& line, Sdp::RtpMap* outRtpmap, int* outPayloadType)
{
    QStringList params = line.split(' ');
    if (params.size() < 2)
        return false; // invalid data format. skip
    QStringList trackInfo = params[0].split(':');
    QStringList codecInfo = params[1].split('/');
    if (trackInfo.size() < 2 || codecInfo.size() < 2)
        return false; // invalid data format

    *outPayloadType = trackInfo[1].toUInt();
    outRtpmap->codecName = codecInfo[0];
    outRtpmap->clockRate = codecInfo[1].toInt();
    if (codecInfo.size() >= 3)
        outRtpmap->channels = codecInfo[2].toInt();
    return true;
}

bool parseFmtp(const QString& line, Sdp::Fmtp* outFmtp, int* outPayloadType)
{
    int valueIndex = line.indexOf(' ');
    if (valueIndex == -1)
        return false;

    QStringList fmtpHeader = line.left(valueIndex).split(':');
    if (fmtpHeader.size() < 2)
        return false;
    *outPayloadType = fmtpHeader[1].toUInt();
    outFmtp->params = line.mid(valueIndex + 1).split(';');
    for (QString& param: outFmtp->params)
        param = param.trimmed();

    return true;
}

Sdp::Media parseMedia(QStringList& lines)
{
    Sdp::Media media;
    QString line = lines.front().trimmed().toLower();
    QStringList trackParams = line.mid(2).split(' ');
    media.mediaType = mediaTypeFromString(trackParams[0]);

    if (trackParams.size() >= 4)
        media.rtpmap.codecName = findCodecById(trackParams[3].toInt());

    if (trackParams.size() >= 2)
        media.serverPort = trackParams[1].toInt();

    if (trackParams.size() >= 4)
        media.payloadType = trackParams[3].toInt();

    lines.pop_front();
    for (; !lines.empty() && !lines.front().startsWith("m=", Qt::CaseInsensitive); lines.pop_front())
    {
        line = lines.front().trimmed();
        if (line.startsWith("a=", Qt::CaseInsensitive))
            media.sdpAttributes << line; // save sdp for codec parser

        if (line.startsWith("a=rtpmap", Qt::CaseInsensitive))
        {
            int payloadType = 0;
            Sdp::RtpMap rtpmap;
            if (parseRtpMap(line, &rtpmap, &payloadType) &&
                (media.payloadType == 0 || payloadType == media.payloadType)) // Ignore invalid rtpmap
            {
                media.rtpmap = rtpmap;
                media.payloadType = payloadType;
                if (rtpmap.codecName.isEmpty())
                    rtpmap.codecName = findCodecById(payloadType);
            }
        }
        else if (line.startsWith("a=fmtp", Qt::CaseInsensitive))
        {
            int payloadType = 0;
            Sdp::Fmtp fmtp;
            if (parseFmtp(line, &fmtp, &payloadType) && payloadType == media.payloadType)
                media.fmtp = fmtp;
        }
        else if (line.startsWith("a=control:", Qt::CaseInsensitive))
        {
            media.control = line.mid(QString("a=control:").length());
        }
        else if (line.startsWith("a=sendonly", Qt::CaseInsensitive))
        {
            media.sendOnly = true;
        }
    }
    return media;
}

static QHostAddress parseServerAddress(const QString& line)
{
    auto fields = line.split(' ');
    if (fields.size() >= 3 && fields[1].toUpper() == "IP4")
        return QHostAddress(fields[2].split('/').front());
    return QHostAddress();
}

void Sdp::parse(const QString& sdpData)
{
    serverAddress.clear();
    controlUrl.clear();
    media.clear();

    QStringList lines = sdpData.split('\n');
    while(!lines.isEmpty())
    {
        QString line = lines.front().trimmed();
        if (line.startsWith("m=", Qt::CaseInsensitive))
        {
            media.push_back(parseMedia(lines));
        }
        else
        {
            const static QString kControlUrlPrefix = "a=control:";
            if (line.startsWith("c=", Qt::CaseInsensitive))
                serverAddress = parseServerAddress(line);
            else if (line.startsWith(kControlUrlPrefix, Qt::CaseInsensitive))
                controlUrl = line.mid(kControlUrlPrefix.length());
            lines.pop_front();
        }
    }
}

} // namespace nx::streaming
