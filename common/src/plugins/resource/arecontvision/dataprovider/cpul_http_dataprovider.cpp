#ifdef ENABLE_ARECONT

#include <utils/common/synctime.h>
#include <utils/common/log.h>

#include "../resource/av_resource.h"
#include "core/datapacket/video_data_packet.h"

#include "cpul_http_dataprovider.h"


AVClientPullSSHTTPStreamreader::AVClientPullSSHTTPStreamreader(const QnResourcePtr& res):
QnPlAVClinetPullStreamReader(res)
{

    m_port = 69;
    m_timeout = 500;

    const QnPlAreconVisionResource* avRes = dynamic_cast<const QnPlAreconVisionResource*>(res.data());

    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
    m_name = avRes->getName();
    m_auth = avRes->getAuth();

}

QnAbstractMediaDataPtr  AVClientPullSSHTTPStreamreader::getNextData()
{

    QString request;

    unsigned int forecast_size = 0;
    int left;
    int top;
    int right;
    int bottom;

    int width, height;

    int quality;
    int bitrate = 0;

    bool resolutionFULL;
    int streamID;

    bool h264;

    {
            QMutexLocker mutex(&m_mutex);

            h264 = isH264();

            if (!m_streamParam.contains("Quality") || !m_streamParam.contains("resolution") ||
                !m_streamParam.contains("image_left") || !m_streamParam.contains("image_top") ||
                !m_streamParam.contains("image_right") || !m_streamParam.contains("image_bottom") ||
                (h264 && !m_streamParam.contains("streamID")) )
            {
                NX_LOG("Erorr!!! parameter is missing in stream params.", cl_logERROR);
                return QnAbstractMediaDataPtr(0);
            }

            //=========
            left = m_streamParam.value("image_left").toInt();
            top = m_streamParam.value("image_top").toInt();
            right = m_streamParam.value("image_right").toInt();
            bottom = m_streamParam.value("image_bottom").toInt();

            width = right - left;
            height = bottom - top;

            quality = m_streamParam.value("Quality").toInt();
            //quality = getQuality();

            resolutionFULL = (m_streamParam.value("resolution").toString() == QLatin1String("full"));
            streamID = 0;
            if (h264)
            {
                streamID = m_streamParam.value("streamID").toInt();
                //bitrate = m_streamParam.get("Bitrate").value.value;
                bitrate = getBitrate();
            }
            //=========

            if (h264)
                quality=37-quality; // for H.264 it's not quality; it's qp

            if (!h264)
                request += QLatin1String("image");
            else
                request += QLatin1String("h264f");

            request += QLatin1String("?res=");
            if (resolutionFULL)
                request += QLatin1String("full");
            else
                request += QLatin1String("half");

            request += QLatin1String("&x0=") + QString::number(left)
                     + QLatin1String("&y0=") + QString::number(top)
                     + QLatin1String("&x1=") + QString::number(right)
                     + QLatin1String("&y1=") + QString::number(bottom);

            if (!h264)
                request += QLatin1String("&quality=");
            else
                request += QLatin1String("&qp=");

            request += QString::number(quality) + QLatin1String("&doublescan=1") + QLatin1String("&ssn=") + QString::number(streamID);

            if (h264)
            {
                if (needKeyData())
                    request += QLatin1String("&iframe=1");

                if (bitrate)
                    request += QLatin1String("&bitrate=") + QString::number(bitrate);
            }

            forecast_size = resolutionFULL ? (width*height)/2  : (width*height)/4; // 0.5 meg per megapixel; to avoid mem realock
    }

    CLSimpleHTTPClient http_client(getResource().dynamicCast<QnPlAreconVisionResource>()->getHostAddress(), m_port, m_timeout, m_auth);

    http_client.doGET(request);

    if (!http_client.isOpened())
        return QnAbstractMediaDataPtr(0);

    QnWritableCompressedVideoDataPtr videoData ( new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT,forecast_size) );
    QnByteArray& img = videoData->m_data;

    while(http_client.isOpened())
    {
        int readed = http_client.read(img.startWriting(2000), 2000);

        if (readed<0) // error
        {
            return QnAbstractMediaDataPtr(0);
        }

        img.finishWriting(readed);

        if (img.size()>CL_MAX_DATASIZE)
        {
            NX_LOG("Image is too big!!", cl_logERROR);
            return QnAbstractMediaDataPtr(0);
        }
    }

    img.removeTrailingZeros();

    //unit delimetr
    if (h264)
    {

        char  c = 0;
        img.write(&c,1); //0
        img.write(&c,1); //0
        c = 1;
        img.write(&c,1); //1
        c = 0x09;
        img.write(&c,1); //0x09
        c = 0x10; // 0x10
        img.write(&c,1); //0x09

    }

    //video/H.264I
    videoData->compressionType = h264 ? CODEC_ID_H264 : CODEC_ID_MJPEG;
    videoData->width = resolutionFULL ? width : width>>1;
    videoData->height = resolutionFULL ? height : height>>1;

    videoData->flags &= AV_PKT_FLAG_KEY;

    if (h264) // for jpej keyFrame always true
    {
        if (!http_client.header().contains("Content-Type")) // very strange
        {
            videoData->flags &= ~AV_PKT_FLAG_KEY;
        }
        else
            if (http_client.header().value("Content-Type") == "video/H.264P")
                videoData->flags &= ~AV_PKT_FLAG_KEY;

    }

    videoData->channelNumber = 0;

    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;

    return videoData;

}

#endif
