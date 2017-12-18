#ifdef ENABLE_ARECONT

#include "panoramic_cpul_tftp_dataprovider.h"

#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#include "av_client_pull.h"
#include "../resource/av_resource.h"
#include "../tools/simple_tftp_client.h"
#include "../tools/AVJpegHeader.h"
#include "nx/streaming/video_data_packet.h"
#include "core/resource/resource_media_layout.h"
#include "../resource/av_panoramic.h"
#include <nx/streaming/config.h>

// ======================================================

extern int create_sps_pps(
    int frameWidth,
    int frameHeight,
    int deblock_filter,
    unsigned char* data, int max_datalen);

extern AVLastPacketSize ExtractSize(const unsigned char* arr);


// =========================================================

AVPanoramicClientPullSSTFTPStreamreader::AVPanoramicClientPullSSTFTPStreamreader(const QnResourcePtr& res):
    QnPlAVClinetPullStreamReader(res)
{

    m_timeout = 500;
    m_last_width = 1600;
    m_last_height = 1200;

    const QnPlAreconVisionResource* avRes = dynamic_cast<const QnPlAreconVisionResource*>(res.data());

    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
    m_model = avRes->getModel();
    m_tftp_client = 0;
    m_motionData = 0;
    m_channelCount = avRes->getVideoLayout()->channelCount();
}

AVPanoramicClientPullSSTFTPStreamreader::~AVPanoramicClientPullSSTFTPStreamreader()
{
    stop();
    delete m_tftp_client;
}

CameraDiagnostics::Result AVPanoramicClientPullSSTFTPStreamreader::diagnoseMediaStreamConnection()
{
    //TODO/IMPL
    return CameraDiagnostics::NotImplementedResult();
}

QnMetaDataV1Ptr AVPanoramicClientPullSSTFTPStreamreader::getCameraMetadata()
{
    QnMetaDataV1Ptr motion(new QnMetaDataV1());
    //Andy Tau & Touch Enable feat. Louisa Allen - Sorry (Sean Truby Remix)
    QString mdresult;

    QnArecontPanoramicResourcePtr res = getResource().dynamicCast<QnArecontPanoramicResource>();

    if (!res->getParamPhysicalByChannel(m_motionData + 1, lit("mdresult"), mdresult))
        return QnMetaDataV1Ptr(0);

    if (mdresult == lit("no motion"))
    {
        motion->channelNumber = m_motionData;
        return motion; // no motion detected
    }


    QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    int zones = avRes->totalMdZones() == 1024 ? 32 : 8;

    QStringList md = mdresult.split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (md.size() < zones*zones)
        return QnMetaDataV1Ptr(0);


    int pixelZoneSize = avRes->getZoneSite() * 32;
    if (pixelZoneSize == 0)
        return QnMetaDataV1Ptr(0);


    QString maxSensorWidth = getResource()->getProperty(lit("MaxSensorWidth"));
    QString maxSensorHight = getResource()->getProperty(lit("MaxSensorHeight"));

    QRect imageRect(0, 0, maxSensorWidth.toInt(), maxSensorHight.toInt());
    QRect zeroZoneRect(0, 0, pixelZoneSize, pixelZoneSize);

    for (int x = 0; x < zones; ++x)
    {
        for (int y = 0; y < zones; ++y)
        {
            int index = y*zones + x;
            QString m = md.at(index);


            if (m == QLatin1String("00") || m == QLatin1String("0"))
                continue;

            QRect currZoneRect = zeroZoneRect.translated(x*pixelZoneSize, y*pixelZoneSize);

            motion->mapMotion(imageRect, currZoneRect);

        }
    }

    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000 * 1000 * 1000; // 1000 sec
    motion->channelNumber = m_motionData;
    filterMotionByMask(motion);
    return motion;
}

QnAbstractMediaDataPtr AVPanoramicClientPullSSTFTPStreamreader::getNextData()
{
    updateCameraParams();

    if (m_motionData > 0)
    {
        --m_motionData;
        QnAbstractMediaDataPtr metadata = getMetaData();
        if (metadata)
            return metadata;
    }

    if (needMetaData())
    {
        m_motionData = m_channelCount;
    }


    QByteArray request;

    bool h264;
    int streamID = 0;

    int width = m_last_width;
    int height = m_last_height;
    bool resolutionFULL = true;

    int quality = 15;

    {
        QnMutexLocker mutex(&m_mutex);

        h264 = isH264();

        if (h264) // cam is not jpeg only
        {
            if (!m_streamParam.contains("streamID"))
            {
                NX_LOG("Erorr!!! parameter is missing in stream params.", cl_logERROR);
                return QnAbstractMediaDataPtr(0);
            }

            streamID = m_streamParam.value("streamID").toInt();
        }
    }

    if (!h264)
        request.append("image");
    else
        request.append("h264?ssn=").append(QByteArray::number(streamID));

    if (h264)
    {
        if (needKeyData())
            request.append(";iframe=1;");
        else
            request.append(";iframe=0;");

    }

    //unsigned int forecast_size = (width*height)/2; // 0.5 meg per megapixel as maximum

    QnPlAreconVisionResourcePtr netRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    if (m_tftp_client == 0 || m_tftp_client->getHostAddress() != netRes->getHostAddress())
    {
        delete m_tftp_client;
        m_tftp_client = new CLSimpleTFTPClient(netRes->getHostAddress(), m_timeout, 3);

        QUrl streamUrl;
        streamUrl.setScheme(lit("tftp"));
        streamUrl.setHost(netRes->getHostAddress());
        streamUrl.setPort(CLSimpleTFTPClient::kDefaultTFTPPort);

        netRes->updateSourceUrl(streamUrl.toString() + lit("/") + request, getRole());
    }

    m_videoFrameBuff.clear();
    QnByteArray& img = m_videoFrameBuff;

    // ==========================================
    int expectable_header_size = 0;
    if (h264)
    {
        // 0) in tftp mode cam do not send image header, we need to form it.
        // 1) h264 header has variable len; depends on actual width and hight;
        // 2) this not very likely, but still possible( for example if camera automatically switched from night to day mode and change resolution): returned resolution is not equal to requested resolution

        // 3) we do not nees to put SPS and PPS in before each, but we can.

        unsigned char h264header[50];
        expectable_header_size = create_sps_pps(resolutionFULL ? width : width / 2, resolutionFULL ? height : height / 2, 0, h264header, sizeof(h264header));
        img.startWriting(expectable_header_size + 5); // 5: from cam in tftp mode we receive data starts from second byte of slice header; so we need to put start code and first byte
        img.finishWriting(expectable_header_size + 5);

        // please note that we did not write a single byte to img, we just move position...
        // we will write header based on last packet information
    }
    else
    {
        // in jpeg image header size has constant len
        img.startWriting(AVJpeg::Header::GetHeaderSize() - 2);
        img.finishWriting(AVJpeg::Header::GetHeaderSize() - 2); //  for some reason first 2 bytes from cam is trash

        // please note that we did not write a single byte to img, we just move position...
        // we will write header based on last packet information
    }

    // ==========================================

    int readed = m_tftp_client->read(QLatin1String(request), img);

    if (readed == 0) // cannot read data
    {
        return QnAbstractMediaDataPtr(0);
    }

    img.removeTrailingZeros();

    int lp_size;
    const unsigned char* last_packet = m_tftp_client->getLastPacket(lp_size);

    int iframe_index;

    if (m_panoramic)
    {
        iframe_index = 89;
    }
    else if (m_dualsensor)
    {
        iframe_index = 98;
    }
    else
        iframe_index = 93;


    AVLastPacketSize size;

    int channelNum = 0;

    if (m_panoramic)
    {
        const unsigned char* arr = last_packet + 0x0C;
        arr[0] & 4 ? resolutionFULL = true : false;
        size = ExtractSize(&arr[2]);

        if (size.width > 5000 || size.width < 0 || size.height>5000 || size.height < 0) // bug of arecontvision firmware
        {
            return QnAbstractMediaDataPtr(0);
        }

        m_last_width = size.width;
        m_last_height = size.height;

        channelNum = arr[0] & 3;

        //multisensor_is_zoomed = arr[0] & 8;
        //IMAGE_RESOLUTION res;
        //arr[0] & 4 ? res = imFULL : res = imHALF;

        quality = arr[1];
    }

    if (h264 && (lp_size < iframe_index))
    {
        NX_LOG("last packet is too short!", cl_logERROR);
        return QnAbstractMediaDataPtr(0);
    }

    bool iFrame = true;

    if (h264)
    {

        if (last_packet[iframe_index - 1] == 0)
            iFrame = false;

        // ==========================================
        //put unit delimetr at the end of the frame

        char  c = 0;
        img.write(&c, 1); //0
        img.write(&c, 1); //0
        c = 1;
        img.write(&c, 1); //1
        c = 0x09;
        img.write(&c, 1); //0x09
        c = (iFrame) ? 0x10 : 0x30;
        img.write(&c, 1); // 0x10

        // ==========================================
        char* dst = img.data();
        //if (videoData->keyFrame) // only if I frame we need SPS&PPS
        {
            unsigned char h264header[50];
            int header_size = create_sps_pps(size.width, size.height, 0, h264header, sizeof(h264header));

            if (header_size != expectable_header_size) // this should be very rarely
            {
                int diff = header_size - expectable_header_size;
                if (diff > 0)
                    img.startWriting(diff);

                NX_LOG("Perfomance hint: AVPanoramicClientPullSSTFTP Streamreader moved received data", cl_logINFO);

                memmove(img.data() + 5 + header_size, img.data() + 5 + expectable_header_size, img.size() - (5 + expectable_header_size));
                img.finishWriting(diff);
            }

            dst = img.data();
            memcpy(dst, h264header, header_size);
            dst += header_size;
        }
        /*
        else
        {
            img.ignore_first_bytes(expectable_header_size); // if you decoder needs compressed data alignment, just do not do it. ffmpeg will delay one frame if do not do it.
            dst+=expectable_header_size;
        }
        */

        // we also need to put very begining of SH
        dst[0] = dst[1] = dst[2] = 0; dst[3] = 1;
        dst[4] = (iFrame) ? 0x65 : 0x41;

        img.startWriting(8);
        dst = img.data() + img.size();
        dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = dst[5] = dst[6] = dst[7] = 0;

    }
    else
    {

        // writes JPEG header at very beginning
        AVJpeg::Header::GetHeader((unsigned char*)img.data(), size.width, size.height, quality, m_model.toLatin1().data());
    }

    if (!qBetween(1U, m_videoFrameBuff.size(), MAX_ALLOWED_FRAME_SIZE))
        return QnAbstractMediaDataPtr();
    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, m_videoFrameBuff.size()));
    videoData->m_data.write(m_videoFrameBuff);


    if (iFrame)
        videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    videoData->compressionType = h264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MJPEG;
    videoData->width = size.width;
    videoData->height = size.height;
    videoData->channelNumber = channelNum;

    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    return videoData;

}

bool AVPanoramicClientPullSSTFTPStreamreader::needKeyData() const
{
    QnMutexLocker mtx(&m_mutex);
    for (int i = 0; i < m_channelCount; ++i)
        if (m_gotKeyFrame[i] < 2)  // due to bug of AV panoramic H.264 cam. cam do not send frame with diff resolution of resolution changed. first I frame comes with old resolution
            return true;

    return false;

}

void AVPanoramicClientPullSSTFTPStreamreader::beforeRun()
{
    QnPlAVClinetPullStreamReader::beforeRun();
    QnArecontPanoramicResourcePtr res = getResource().dynamicCast<QnArecontPanoramicResource>();
    res->updateFlipState();
}

#endif
