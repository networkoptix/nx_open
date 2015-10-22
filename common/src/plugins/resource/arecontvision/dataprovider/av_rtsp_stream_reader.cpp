/**********************************************************
* Oct 22, 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ARECONT

#include "av_rtsp_stream_reader.h"

#include <utils/common/log.h>

#include "../resource/av_resource.h"


QnArecontRtspStreamReader::QnArecontRtspStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_rtpStreamParser(res)
{
}

QnArecontRtspStreamReader::~QnArecontRtspStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnArecontRtspStreamReader::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    const auto requestStr = generateRequestString(params);

    const Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);

    auto res = getResource().dynamicCast<QnPlAreconVisionResource>();
    const QString url = lit("rtsp://%1:%2/%3").arg(res->getHostAddress()).arg(554).arg(requestStr);

    m_rtpStreamParser.setRequest(url);
    return m_rtpStreamParser.openStream();
}

void QnArecontRtspStreamReader::closeStream()
{
    m_rtpStreamParser.closeStream();
}

bool QnArecontRtspStreamReader::isStreamOpened() const
{
    return m_rtpStreamParser.isStreamOpened();
}

QnMetaDataV1Ptr QnArecontRtspStreamReader::getCameraMetadata()
{
    return QnMetaDataV1Ptr(0);
}

void QnArecontRtspStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    m_rtpStreamParser.pleaseStop();
}

QnAbstractMediaDataPtr QnArecontRtspStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez) 
        {
            //QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(rez);
            //ToDo: if (videoData)
            //    parseMotionInfo(videoData);

            //if (!videoData || isGotFrame(videoData))
            break;
        }
        else {
            errorCount++;
            if (errorCount > 1) {
                closeStream();
                break;
            }
        }
    }

    return rez;
}

QnConstResourceAudioLayoutPtr QnArecontRtspStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

QString QnArecontRtspStreamReader::generateRequestString(const QnLiveStreamParams& params)
{
    static const int SECOND_STREAM_FPS = 5;

    QString str;

    str += lit("h264.sdp?");
    if (getRole() == Qn::CR_LiveVideo)
    {
        str += lit("Res=full");
        str += lit("&FPS=%1").arg((int)params.fps);
    }
    else
    {
        str += lit("Res=half");
        str += lit("&FPS=%1").arg(SECOND_STREAM_FPS);
        str += lit("&QP=37");
    }

    return str;
}

#endif // ENABLE_ARECONT
