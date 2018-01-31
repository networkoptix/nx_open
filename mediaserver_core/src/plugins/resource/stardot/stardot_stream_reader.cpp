#ifdef ENABLE_STARDOT

#include "stardot_stream_reader.h"

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include "utils/media/nalUnits.h"
#include "network/tcp_connection_priv.h"

#include <motion/motion_detection.h>

#include "stardot_resource.h"

#if 0
static const char STARDOT_SEI_UUID[] = "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa";
static const int STARDOT_SEI_PRODUCT_INFO = 0x0a00;
static const int STARDOT_SEI_TIMESTAMP = 0x0a01;
static const int STARDOT_SEI_TRIGGER_DATA = 0x0a03;
#endif
static const QByteArray STARDOT_MOTION_UUID = QByteArray::fromHex("bed9b7e0f06032a1bb7e7d4c6d82d249");


QnStardotStreamReader::QnStardotStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_multiCodec(res)
{
    m_stardotRes = res.dynamicCast<QnStardotResource>();
}

QnStardotStreamReader::~QnStardotStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnStardotStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& liveStreamParams)
{
    // configure stream params

    // get URL

    QnLiveStreamParams params = liveStreamParams;
    // TODO: advanced params control is not implemented for this driver yet
    params.resolution = m_stardotRes->getResolution();
    if (isCameraControlRequired)
    {
        QString request(lit("admin.cgi?image&h264_bitrate=%1&h264_framerate=%2"));
        int bitrate = m_stardotRes->suggestBitrateKbps(params, getRole());
        request = request.arg(bitrate).arg(params.fps);

        CLHttpStatus status;
        m_stardotRes->makeStardotRequest(request, status);
        if (status != CL_HTTP_SUCCESS)
        {
            if (status == CL_HTTP_AUTH_REQUIRED)
            {
                m_resource->setStatus(Qn::Unauthorized);
                QUrl requestedUrl;
                requestedUrl.setHost( m_stardotRes->getHostAddress() );
                requestedUrl.setPort( m_stardotRes->httpPort() );
                requestedUrl.setScheme( QLatin1String("http") );
                int queryPos = request.indexOf(L'?');
                requestedUrl.setPath(request.left(queryPos));
                requestedUrl.setQuery(request.mid(queryPos+1));
                return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
            }
            return CameraDiagnostics::RequestFailedResult(QLatin1String("admin.cgi?image"), QLatin1String(nx::network::http::StatusCode::toString((nx::network::http::StatusCode::Value)status)));
        }
    }

    QString streamUrl = m_stardotRes->getRtspUrl();
    m_stardotRes->updateSourceUrl(streamUrl, getRole());
    NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(streamUrl).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);
    m_multiCodec.setRole(getRole());
    m_multiCodec.setRequest(streamUrl);
	m_stardotRes->updateSourceUrl(m_multiCodec.getCurrentStreamUrl(), getRole());
    const CameraDiagnostics::Result result = m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED)
        m_resource->setStatus(Qn::Unauthorized);
    return result;
}

void QnStardotStreamReader::closeStream()
{
    m_multiCodec.closeStream();
}

bool QnStardotStreamReader::isStreamOpened() const
{
    return m_multiCodec.isStreamOpened();
}


void QnStardotStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    m_multiCodec.pleaseStop();
}

QnAbstractMediaDataPtr QnStardotStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetadata())
        return getMetadata();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();

    if (rez) {
        if (rez->dataType == QnAbstractMediaData::VIDEO)
            parseMotionInfo(std::dynamic_pointer_cast<QnCompressedVideoData>(rez));
    }
    else {
        closeStream();
    }

    return rez;
}

QnConstResourceAudioLayoutPtr QnStardotStreamReader::getDPAudioLayout() const
{
    return m_multiCodec.getAudioLayout();
}

// motion estimation

void QnStardotStreamReader::parseMotionInfo(const QnCompressedVideoDataPtr& videoData)
{
    const quint8* curNal = (const quint8*) videoData->data();
    const quint8* end = curNal + videoData->dataSize();
    curNal = NALUnit::findNextNAL(curNal, end);
    //int prefixSize = 3;
    //if (end - curNal >= 4 && curNal[2] == 0)
    //    prefixSize = 4;

    const quint8* nextNal = curNal;
    for (;curNal < end; curNal = nextNal)
    {
        nextNal = NALUnit::findNextNAL(curNal, end);
        quint8 nalUnitType = *curNal & 0x1f;
        if (nalUnitType != nuSEI)
            continue;
        // parse SEI message
        SPSUnit sps;
        SEIUnit sei;
        sei.decodeBuffer(curNal, nextNal);
        sei.deserialize(sps, 0);
        for (int i = 0; i < sei.m_userDataPayload.size(); ++i)
        {
            const quint8* payload = sei.m_userDataPayload[i].first;
            int len = sei.m_userDataPayload[i].second;
            if (len < 48)
                continue;
            if (QByteArray::fromRawData((const char*)payload, 16) != STARDOT_MOTION_UUID)
                continue;
            processMotionBinData(payload+16, videoData->timestamp);
        }
    }
}

void QnStardotStreamReader::processMotionBinData(const quint8* data, qint64 timestamp)
{
    if (m_lastMetadata == 0) {
        m_lastMetadata = QnMetaDataV1Ptr(new QnMetaDataV1());
        m_lastMetadata->m_duration = 1000*1000*10; // 10 sec
        m_lastMetadata->timestamp = timestamp;
    }

    // rotate source data to 90 degree and scale it from 16x16 to 44x32. Destination data stored in native byte order instead of network order
    quint32* dst = (quint32*) m_lastMetadata->m_data.data();
    quint32 dstMask = 0xc0000000;
    for (int y = 0; y < 16; ++y)
    {
        quint16 srcLine = (data[y*2] << 8) + data[y*2+1];
        quint16 srcMask = 0x8000;
        for (int x = 0; x < 16; ++x)
        {
            if (srcLine & srcMask) {
                int dstX1 = (x * Qn::kMotionGridWidth + 8)/16;
                int dstX2 = ((x+1) * Qn::kMotionGridWidth + 8)/16;
                for (int dstX = dstX1; dstX < dstX2; ++dstX)
                    dst[dstX] |= dstMask;
            }
            srcMask >>= 1;
        }
        dstMask >>= 2;
    }
}

QnMetaDataV1Ptr QnStardotStreamReader::getCameraMetadata()
{
    QnMetaDataV1Ptr rez;
    if (m_lastMetadata) {
        quint32* dst = (quint32*) m_lastMetadata->m_data.data();
        for (int i = 0; i < Qn::kMotionGridWidth; ++i)
            dst[i] = htonl(dst[i]);
        const simd128i* mask = m_stardotRes->getMotionMaskBinData();
        if (mask)
            m_lastMetadata->removeMotion(mask);

        rez.swap( m_lastMetadata );
    }
    return rez;


    //QnMetaDataV1Ptr rez = m_lastMetadata != 0 ? m_lastMetadata : QnMetaDataV1Ptr(new QnMetaDataV1());
    //m_lastMetadata.clear();
    //return rez;
}

#endif // #ifdef ENABLE_STARDOT
