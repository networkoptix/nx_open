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

bool parseRtpMap(const QString& line, Sdp::RtpMap& rtpmap)
{
    QStringList params = line.split(' ');
    if (params.size() < 2)
        return false; // invalid data format. skip
    QStringList trackInfo = params[0].split(':');
    QStringList codecInfo = params[1].split('/');
    if (trackInfo.size() < 2 || codecInfo.size() < 2)
        return false; // invalid data format

    rtpmap.payloadType = trackInfo[1].toUInt();
    rtpmap.codecName = codecInfo[0];
    rtpmap.clockRate = codecInfo[1].toInt();
    if (codecInfo.size() >= 3)
        rtpmap.channels = codecInfo[2].toInt();
    return true;
}

bool parseFmtp(const QString& line, Sdp::Fmtp& fmtp)
{
    int valueIndex = line.indexOf(' ');
    if (valueIndex == -1)
        return false;

    QStringList fmtpHeader = line.left(valueIndex).split(':');
    if (fmtpHeader.size() < 2)
        return false;
    fmtp.payloadType = fmtpHeader[1].toUInt();
    fmtp.params = line.mid(valueIndex + 1).split(';');
    for (QString& param: fmtp.params)
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
        media.payloadType = trackParams[3].toInt();

    lines.pop_front();
    for (; !lines.empty() && !lines.front().startsWith("m=", Qt::CaseInsensitive); lines.pop_front())
    {
        line = lines.front().trimmed();
        if (line.startsWith("a=", Qt::CaseInsensitive))
            media.sdpAttributes << line.toLower(); // save sdp for codec parser

        if (line.startsWith("a=rtpmap", Qt::CaseInsensitive))
        {
            Sdp::RtpMap rtpmap;
            if (parseRtpMap(line, rtpmap) &&
                (media.payloadType == 0 || rtpmap.payloadType == media.payloadType)) // Ignore invalid rtpmap
            {
                media.rtpmap = rtpmap;
                media.payloadType = rtpmap.payloadType;
                if (rtpmap.codecName.isEmpty())
                    rtpmap.codecName = findCodecById(rtpmap.payloadType);
            }
        }
        else if (line.startsWith("a=fmtp", Qt::CaseInsensitive))
        {
            Sdp::Fmtp fmtp;
            if (parseFmtp(line, fmtp) && fmtp.payloadType == media.payloadType)
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

void Sdp::parse(const QString& sdpData)
{
    media.clear();
    QStringList lines = sdpData.split('\n');
    while(!lines.isEmpty())
    {
        QString line = lines.front().trimmed();
        if (line.startsWith("m=", Qt::CaseInsensitive))
            media.push_back(parseMedia(lines));
        else
            lines.pop_front();
    }
}

} // namespace nx::streaming
