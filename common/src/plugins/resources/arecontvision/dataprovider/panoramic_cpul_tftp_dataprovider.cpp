#include "av_client_pull.h"
#include "panoramic_cpul_tftp_dataprovider.h"
#include "../resource/av_resource.h"
#include "../tools/simple_tftp_client.h"
#include "../tools/AVJpegHeader.h"
#include "core/resource/resource_media_layout.h"
#include "utils/common/synctime.h"
#include "../resource/av_panoramic.h"

//======================================================

extern int create_sps_pps(
                   int frameWidth,
                   int frameHeight,
                   int deblock_filter,
                   unsigned char* data, int max_datalen);

extern AVLastPacketSize ExtractSize(const unsigned char* arr);


//=========================================================

AVPanoramicClientPullSSTFTPStreamreader ::AVPanoramicClientPullSSTFTPStreamreader(QnResourcePtr res):
QnPlAVClinetPullStreamReader(res)
{

    m_timeout = 500;
    m_last_width = 1600;
    m_last_height = 1200;

    QnPlAreconVisionResourcePtr avRes = res.dynamicCast<QnPlAreconVisionResource>();

    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
    m_name = avRes->getName();
    m_tftp_client = 0;

    m_motionData = 0;
}

AVPanoramicClientPullSSTFTPStreamreader::~AVPanoramicClientPullSSTFTPStreamreader()
{
    stop();
    delete m_tftp_client;
}


QnMetaDataV1Ptr AVPanoramicClientPullSSTFTPStreamreader::getMetaData()
{
    QnMetaDataV1Ptr motion(new QnMetaDataV1());
    //Andy Tau & Touch Enable feat. Louisa Allen - Sorry (Sean Truby Remix)
    QVariant mdresult;

    QnArecontPanoramicResourcePtr res = getResource().dynamicCast<QnArecontPanoramicResource>();

    if (!res->getParamPhysical(m_motionData + 1, "MdResult", mdresult))
        return QnMetaDataV1Ptr(0);

    if (mdresult.toString() == QLatin1String("no motion"))
        return motion; // no motion detected


    QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    int zones = avRes->totalMdZones() == 1024 ? 32 : 8;

    QStringList md = mdresult.toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (md.size() < zones*zones)
        return QnMetaDataV1Ptr(0);


    QVariant zone_size;
    if (!getResource()->getParam("Zone size", zone_size, QnDomainMemory))
        return QnMetaDataV1Ptr(0);

    int pixelZoneSize = zone_size.toInt() * 32;


    QVariant maxSensorWidth;
    QVariant maxSensorHight;
    getResource()->getParam("MaxSensorWidth", maxSensorWidth, QnDomainMemory);
    getResource()->getParam("MaxSensorHeight", maxSensorHight, QnDomainMemory);


    QRect imageRect(0, 0, maxSensorWidth.toInt(), maxSensorHight.toInt());
    QRect zeroZoneRect(0, 0, pixelZoneSize, pixelZoneSize);

    for (int x = 0; x < zones; ++x)
    {
        for (int y = 0; y < zones; ++y)
        {
            int index = y*zones + x;
            QString m = md.at(index) ;


            if (m == "00" || m == "0")
                continue;

            QRect currZoneRect = zeroZoneRect.translated(x*pixelZoneSize, y*pixelZoneSize);

            motion->mapMotion(imageRect, currZoneRect);

        }
    }

    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000*1000*1000; // 1000 sec 
    motion->channelNumber = m_motionData;

    return motion;
}

QnAbstractMediaDataPtr AVPanoramicClientPullSSTFTPStreamreader::getNextData()
{

    if (m_motionData > 0)
    {
        --m_motionData;
        return getMetaData();
    }

    if (needMetaData())
    {
        m_motionData = 4;
    }


    QByteArray request;

    unsigned int forecast_size = 0;

    bool h264;
    int streamID = 0;

    int width = m_last_width;
    int height = m_last_height;
    bool resolutionFULL = true;

    int quality = 15;

    {
        QMutexLocker mutex(&m_mutex);

        h264 = isH264();;

        if (h264) // cam is not jpeg only
        {
            if (!m_streamParam.contains("streamID"))
            {
                cl_log.log("Erorr!!! parameter is missing in stream params.", cl_logERROR);
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

    forecast_size = (width*height)/2; // 0.5 meg per megapixel as maximum

    QnPlAreconVisionResourcePtr netRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    if (m_tftp_client == 0 || m_tftp_client->getHostAddress() != netRes->getHostAddress()) {
        delete m_tftp_client;
        m_tftp_client = new CLSimpleTFTPClient(netRes->getHostAddress(),  m_timeout, 3);
    }

    m_videoFrameBuff.clear();
    CLByteArray& img = m_videoFrameBuff;

    //==========================================
    int expectable_header_size;
    if (h264)
    {
        // 0) in tftp mode cam do not send image header, we need to form it.
        // 1) h264 header has variable len; depends on actual width and hight;
        // 2) this not very likely, but still possible( for example if camera automatically switched from night to day mode and change resolution): returned resolution is not equal to requested resolution

        // 3) we do not nees to put SPS and PPS in before each, but we can.

        unsigned char h264header[50];
        expectable_header_size = create_sps_pps(resolutionFULL ? width : width/2, resolutionFULL ? height: height/2, 0, h264header, sizeof(h264header));
        img.prepareToWrite(expectable_header_size + 5 ); // 5: from cam in tftp mode we receive data starts from second byte of slice header; so we need to put start code and first byte
        img.done(expectable_header_size + 5 );

        // please note that we did not write a single byte to img, we just move position...
        // we will write header based on last packet information
    }
    else
    {
        // in jpeg image header size has constant len
        img.prepareToWrite(AVJpeg::Header::GetHeaderSize()-2);
        img.done(AVJpeg::Header::GetHeaderSize()-2); //  for some reason first 2 bytes from cam is trash

        // please note that we did not write a single byte to img, we just move position...
        // we will write header based on last packet information
    }

    //==========================================

    int readed = m_tftp_client->read(request, img);

    if (readed == 0) // cannot read data
    {
        return QnAbstractMediaDataPtr(0);
    }

    img.removeZerosAtTheEnd();

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

    if(m_panoramic)
    {
        const unsigned char* arr = last_packet + 0x0C;
        arr[0] & 4 ? resolutionFULL = true : false;
        size = ExtractSize(&arr[2]);

        if (size.width>5000 || size.width<0 || size.height>5000 || size.height<0) // bug of arecontvision firmware
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
        cl_log.log("last packet is too short!", cl_logERROR);
        return QnAbstractMediaDataPtr(0);
    }

    bool iFrame = true;
    
    if (h264)
    {

        if (last_packet[iframe_index-1]==0)
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
        //if (videoData->keyFrame) // only if I frame we need SPS&PPS
        {
            unsigned char h264header[50];
            int header_size = create_sps_pps(size.width, size.height, 0, h264header, sizeof(h264header));

            if (header_size!=expectable_header_size) // this should be very rarely
            {
                int diff = header_size - expectable_header_size;
                if (diff>0)
                    img.prepareToWrite(diff);

                cl_log.log("Perfomance hint: AVPanoramicClientPullSSTFTP Streamreader moved received data", cl_logINFO);

                memmove(img.data() + 5 + header_size, img.data() + 5 + expectable_header_size, img.size() - (5 + expectable_header_size));
                img.done(diff);
            }

            dst = img.data();
            memcpy(dst, h264header, header_size);
            dst+= header_size;
        }
        /*
        else
        {
            img.ignore_first_bytes(expectable_header_size); // if you decoder needs compressed data alignment, just do not do it. ffmpeg will delay one frame if do not do it.
            dst+=expectable_header_size;
        }
        /**/

        // we also need to put very begining of SH
        dst[0] = dst[1] = dst[2] = 0; dst[3] = 1;
        dst[4] = (iFrame) ? 0x65 : 0x41;

        img.prepareToWrite(8);
        dst = img.data() + img.size();
        dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = dst[5] =  dst[6] = dst[7] = 0;

    }
    else
    {

        // writes JPEG header at very beginning
        AVJpeg::Header::GetHeader((unsigned char*)img.data(), size.width, size.height, quality, m_name.toLatin1().data());
    }

    QnCompressedVideoDataPtr videoData( new QnCompressedVideoData(CL_MEDIA_ALIGNMENT,m_videoFrameBuff.size()) );
    videoData->data.write(m_videoFrameBuff);
    

    if (iFrame)
        videoData->flags |= AV_PKT_FLAG_KEY;

    videoData->compressionType = h264 ? CODEC_ID_H264 : CODEC_ID_MJPEG;
    videoData->width = size.width;
    videoData->height = size.height;
    videoData->channelNumber = channelNum;

    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;

    return videoData;

}

bool AVPanoramicClientPullSSTFTPStreamreader::needKeyData() const
{
    QMutexLocker mtx(&m_mutex);
    for (int i = 0; i < 4; ++i)
        if (m_gotKeyFrame[i]<2)  // due to bug of AV panoramic H.264 cam. cam do not send frame with diff resolution of resolution changed. first I frame comes with old resolution
            return true;

    return false;

}
