#ifdef ENABLE_ARECONT

#include "cpul_tftp_dataprovider.h"

#include <QtCore/QMutex>

#include <utils/common/log.h>
#include <utils/common/util.h>
#include <utils/common/synctime.h>

#include <core/datapacket/video_data_packet.h>
#include <core/dataprovider/media_streamdataprovider.h>

#include "../resource/av_resource.h"
#include "../tools/simple_tftp_client.h"
#include "../tools/AVJpegHeader.h"

#include "av_client_pull.h"


//======================================================

extern int create_sps_pps(
                   int frameWidth,
                   int frameHeight,
                   int deblock_filter,
                   unsigned char* data, int max_datalen);

AVLastPacketSize ExtractSize(const unsigned char* arr)
{
    const int fc = 256;
    AVLastPacketSize size;
    size.x0 = static_cast<unsigned char>(arr[0]) * fc + static_cast<unsigned char>(arr[1]),
        size.y0 = static_cast<unsigned char>(arr[2]) * fc + static_cast<unsigned char>(arr[3]);
    size.width = static_cast<unsigned char>(arr[4]) * fc + static_cast<unsigned char>(arr[5]) - size.x0,
        size.height = static_cast<unsigned char>(arr[6]) * fc + static_cast<unsigned char>(arr[7]) - size.y0;
    return size;
}

//=========================================================

AVClientPullSSTFTPStreamreader::AVClientPullSSTFTPStreamreader(const QnResourcePtr& res):
QnPlAVClinetPullStreamReader(res),
m_black_white(false),
m_prevDataReadResult(CameraDiagnostics::ErrorCode::noError)
{
    m_timeout = 500;

    m_last_width = 0;
    m_last_height = 0;

    m_last_cam_width = 0;
    m_last_cam_height = 0;

    QnPlAreconVisionResourcePtr avRes = res.dynamicCast<QnPlAreconVisionResource>();

    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
    m_model = avRes->getModel();
    m_tftp_client = 0;
}

AVClientPullSSTFTPStreamreader::~AVClientPullSSTFTPStreamreader()
{
    stop();
    delete m_tftp_client;
}

CameraDiagnostics::Result AVClientPullSSTFTPStreamreader::diagnoseMediaStreamConnection()
{
    QMutexLocker lk( &m_mutex );
    return m_prevDataReadResult;
}

QnAbstractMediaDataPtr AVClientPullSSTFTPStreamreader::getNextData()
{
    updateCameraParams();

    if (needMetaData())
    {
        const QnAbstractMediaDataPtr& metadata = getMetaData();
        if (metadata)
            return metadata;
    }

    QString request;


    int left;
    int top;
    int right;
    int bottom;

    int width;
    int height;

    int quality;

    bool resolutionFULL;
    int streamID;

    int bitrate = 0;

    bool h264 =  isH264();

    {
            QMutexLocker mutex(&m_mutex);

            if (!m_streamParam.contains("Quality") || !m_streamParam.contains("resolution") ||
                !m_streamParam.contains("image_left") || !m_streamParam.contains("image_top") ||
                !m_streamParam.contains("image_right") || !m_streamParam.contains("image_bottom") ||
                (h264 && !m_streamParam.contains("streamID")))
            {
                NX_LOG("Error!!! parameter is missing in stream params.", cl_logERROR);
                //return QnAbstractMediaDataPtr(0);
            }

            //=========
            left = m_streamParam.value("image_left").toInt();
            top = m_streamParam.value("image_top").toInt();
            right = m_streamParam.value("image_right").toInt();
            bottom = m_streamParam.value("image_bottom").toInt();

            if (m_dualsensor && m_black_white) //3130 || 3135
            {
                right = right/3*2;
                bottom = bottom/3*2;

                right = right/32*32;
                bottom = bottom/16*16;

                right = qMin(1280, right);
                bottom = qMin(1024, bottom);

            }

            //right = 1280;
            //bottom = 1024;

            //right/=2;
            //bottom/=2;

            width = right - left;
            height = bottom - top;

            if (m_last_cam_width==0)
                m_last_cam_width = width;

            if (m_last_cam_height==0)
                m_last_cam_height= height;

            quality = m_streamParam.value("Quality").toInt();
            //quality = getQuality();

            resolutionFULL = (m_streamParam.value("resolution").toString() == QLatin1String("full"));

            streamID = 0;
            if (h264)
            {
                if (width!=m_last_width || height!=m_last_height || m_last_resolution!=resolutionFULL)
                {
                    // if this is H.264 and if we changed image size, we need to request I frame.
                    // camera itself shoud end I frame. but just in case.. to be on the save side...

                    m_last_resolution = resolutionFULL;
                    m_last_width = width;
                    m_last_height = height;

                    setNeedKeyData();
                }

                streamID = m_streamParam.value("streamID").toInt();
                //bitrate = m_streamParam.value("Bitrate").toInt();
                bitrate = getBitrate();
            }
            //=========

    }

    if (h264)
        quality=37-quality; // for H.264 it's not quality; it's qp

    if (!h264)
        request += QLatin1String("image");
    else
        request += QLatin1String("h264");

    request += QLatin1String("?res=");
    if (resolutionFULL)
        request += QLatin1String("full");
    else
        request += QLatin1String("half");

    request += QLatin1String(";x0=") + QString::number(left)
             + QLatin1String(";y0=") + QString::number(top)
             + QLatin1String(";x1=") + QString::number(right)
             + QLatin1String(";y1=") + QString::number(bottom);

    if (!h264)
        request += QLatin1String(";quality=");
    else
        request += QLatin1String(";qp=");

    request += QString::number(quality) + QLatin1String(";doublescan=0") + QLatin1String(";ssn=") + QString::number(streamID);

    //h264?res=full;x0=0;y0=0;x1=1600;y1=1184;qp=27;doublescan=0;iframe=0;ssn=574;netasciiblksize1450
    //request = "image?res=full;x0=0;y0=0;x1=800;y1=600;quality=10;doublescan=0;ssn=4184;";

    if (h264)
    {
        if (needKeyData())
            request += QLatin1String(";iframe=1;");
        else
            request += QLatin1String(";iframe=0;");

        if (bitrate)
            request += QLatin1String("bitrate=") + QString::number(bitrate) + QLatin1Char(';');
    }

    QnPlAreconVisionResourcePtr netRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    if (m_tftp_client == 0 || m_tftp_client->getHostAddress() != netRes->getHostAddress()) {
        delete m_tftp_client;
        m_tftp_client = new CLSimpleTFTPClient(netRes->getHostAddress(),  m_timeout, 3);
    }

    m_videoFrameBuff.clear();
    
    QnByteArray& img = m_videoFrameBuff;

    //==========================================
    int expectable_header_size = 0;
    if (h264)
    {
        // 0) in tftp mode cam do not send image header, we need to form it.
        // 1) h264 header has variable len; depends on actual width and hight;
        // 2) this not very likely, but still possible( for example if camera automatically switched from night to day mode and change resolution): returned resolution is not equal to requested resolution

        // 3) we do not nees to put SPS and PPS in before each, but we can.

        unsigned char h264header[50];
        expectable_header_size = create_sps_pps(m_last_cam_width , m_last_cam_height, 0, h264header, sizeof(h264header));
        img.startWriting(expectable_header_size + 5 ); // 5: from cam in tftp mode we receive data starts from second byte of slice header; so we need to put start code and first byte
        img.finishWriting(expectable_header_size + 5 );

        // please note that we did not write a single byte to img, we just move position...
        // we will write header based on last packet information
    }
    else
    {
        // in jpeg image header size has constant len
        img.startWriting(AVJpeg::Header::GetHeaderSize()-2);
        img.finishWriting(AVJpeg::Header::GetHeaderSize()-2); //  for some reason first 2 bytes from cam is trash

        // please note that we did not write a single byte to img, we just move position...
        // we will write header based on last packet information
    }

    //==========================================

    int readed = m_tftp_client->read(request, img);
    {
        QMutexLocker lk( &m_mutex );
        m_prevDataReadResult = m_tftp_client->prevIOResult();
    }

    if (readed == 0) // cannot read data
        return QnCompressedVideoDataPtr(0);

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


    if (m_dualsensor)
    {
        if (lp_size<21)
        {
            NX_LOG("last packet is too short!", cl_logERROR);
            return QnAbstractMediaDataPtr(0);
        }

        m_black_white = last_packet[20];
    }

    if (h264 && (lp_size < iframe_index))
    {
        NX_LOG("last packet is too short!", cl_logERROR);
        //delete videoData;
        return QnAbstractMediaDataPtr(0);
    }

    AVLastPacketSize size;

    if(m_dualsensor)
    {
        size = ExtractSize(last_packet + 12);
    }
    else
    {
        const unsigned char* arr = last_packet + 0x0C + 4 + 64;

        arr[0] & 4 ? resolutionFULL = true: false;
        size = ExtractSize(&arr[2]);

        if(!size.width && m_model.contains(QLatin1String("3100")))
            size.width = 2048;

        if(!resolutionFULL)
        {
            size.width /= 2;
            size.height /= 2;
        }

    }

    m_last_cam_width = size.width;
    m_last_cam_height = size.height;

    bool iFrame = true;
    if (h264)
    {

        if (last_packet[iframe_index-1] == 0)
            iFrame = false;

        //==========================================
        //put unit delimetr at the end of the frame

        char  c = 0;
        img.write(&c,1); //0
        img.write(&c,1); //0
        c = 1;
        img.write(&c,1); //1
        c = 0x09;
        img.write(&c,1); //0x09
        c = (iFrame) ? 0x10 : 0x30;
        img.write(&c,1); // 0x10

        //==========================================
        char* dst = img.data();
        if (iFrame) // only if I frame we need SPS&PPS
        {
            unsigned char h264header[50];
            int header_size = create_sps_pps(size.width, size.height, 0, h264header, sizeof(h264header));

            if (header_size!=expectable_header_size) // this should be very rarely
            {
                int diff = header_size - expectable_header_size;
                if (diff>0)
                    img.startWriting(diff);

                NX_LOG("Perfomance hint: AVClientPullSSTFTP streamreader moved received data", cl_logINFO);

                memmove(img.data() + 5 + header_size, img.data() + 5 + expectable_header_size, img.size() - (5 + expectable_header_size));
                img.finishWriting(diff);
            }

            dst = img.data();
            memcpy(dst, h264header, header_size);
            dst += header_size;
        }
        else
        {
            img.ignore_first_bytes(expectable_header_size); // if you decoder needs compressed data alignment, just do not do it. ffmpeg will delay one frame if do not do it.
            dst += expectable_header_size;
        }

        // we also need to put very begining of SH
        dst[0] = dst[1] = dst[2] = 0; dst[3] = 1;
        dst[4] = (iFrame) ? 0x65 : 0x41;

        img.startWriting(8);
        dst = img.data() + img.size();
        dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = dst[5] =  dst[6] = dst[7] = 0;

    }
    else
    {
        // writes JPEG header at very begining
        AVJpeg::Header::GetHeader((unsigned char*)img.data(), size.width, size.height, quality, m_model.toLatin1().data());
    }


    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT,m_videoFrameBuff.size()));

    QnByteArray& imgToSend = videoData->m_data;
    imgToSend.write(m_videoFrameBuff);
    

    if (iFrame)
        videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    videoData->compressionType = h264 ? CODEC_ID_H264 : CODEC_ID_MJPEG;
    videoData->width = size.width;
    videoData->height = size.height;

    videoData->channelNumber = 0;

    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    return videoData;

}

QnMetaDataV1Ptr AVClientPullSSTFTPStreamreader::getCameraMetadata()
{
    QnMetaDataV1Ptr motion(new QnMetaDataV1());
    QString mdresult;
    if (!getResource()->getParamPhysical(lit("mdresult"), mdresult))
        return QnMetaDataV1Ptr(0);

    if (mdresult == lit("no motion"))
        return motion; // no motion detected


    const QnPlAreconVisionResource* avRes = dynamic_cast<QnPlAreconVisionResource*>(getResource().data());
    int zones = avRes->totalMdZones() == 1024 ? 32 : 8;

    QStringList md = mdresult.split(L' ', QString::SkipEmptyParts);
    if (md.size() < zones*zones)
        return QnMetaDataV1Ptr(0);


    QString zone_size = getResource()->getProperty(lit("Zone size"));
    if (zone_size.isEmpty())
        return QnMetaDataV1Ptr(0);

    int pixelZoneSize = zone_size.toInt() * 32;


    QVariant maxSensorWidth = getResource()->getProperty(lit("MaxSensorWidth"));
    QVariant maxSensorHight = getResource()->getProperty(lit("MaxSensorHeight"));
    
    QRect imageRect(0, 0, maxSensorWidth.toInt(), maxSensorHight.toInt());
    QRect zeroZoneRect(0, 0, pixelZoneSize, pixelZoneSize);

    for (int x = 0; x < zones; ++x)
    {
        for (int y = 0; y < zones; ++y)
        {
            int index = y*zones + x;
            QString m = md.at(index) ;


            if (m == lit("00") || m == lit("0"))
                continue;

            QRect currZoneRect = zeroZoneRect.translated(x*pixelZoneSize, y*pixelZoneSize);

            motion->mapMotion(imageRect, currZoneRect);

        }
    }

    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000*1000*1000; // 1000 sec 
    filterMotionByMask(motion);
    return motion;
}

#endif
