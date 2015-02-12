#include "live_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/resource/camera_resource.h"
#include "utils/media/jpeg_utils.h"
#include "utils/media/nalUnits.h"


static const int CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES = 1000;

QnLiveStreamProvider::QnLiveStreamProvider(const QnResourcePtr& res):
    QnAbstractMediaStreamDataProvider(res),
    m_livemutex(QMutex::Recursive),
    m_quality(Qn::QualityNormal),
    m_qualityUpdatedAtLeastOnce(false),
    m_fps(-1.0),
    m_framesSinceLastMetaData(0),
    m_softMotionRole(Qn::CR_Default),
    m_softMotionLastChannel(0),
    m_secondaryQuality(Qn::SSQualityNotDefined),
    m_framesSincePrevMediaStreamCheck(CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES+1),
    m_owner(0)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskBinData[i] = (simd128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
        memset(m_motionMaskBinData[i], 0, MD_WIDTH * MD_HEIGHT/8);
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        m_motionEstimation[i].setChannelNum(i);
#endif
    }

    m_role = Qn::CR_LiveVideo;
    m_timeSinceLastMetaData.restart();
    m_layout = QnConstResourceVideoLayoutPtr();
    m_cameraRes = res.dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(m_cameraRes);
    m_prevCameraControlDisabled = m_cameraRes->isCameraControlDisabled();
    m_layout = m_cameraRes->getVideoLayout();
    m_isPhysicalResource = res.dynamicCast<QnPhysicalCameraResource>();
    m_resolutionCheckTimer.invalidate();
}

void QnLiveStreamProvider::setOwner(QnAbstractVideoCamera* owner)
{
    m_owner = owner;
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
        qFreeAligned(m_motionMaskBinData[i]);
}

void QnLiveStreamProvider::setRole(Qn::ConnectionRole role)
{
    bool needUpdate = false;
    QnAbstractMediaStreamDataProvider::setRole(role);
    {
        QMutexLocker mtx(&m_livemutex);
        updateSoftwareMotion();

        if (role == Qn::CR_SecondaryLiveVideo)
        {
            Qn::StreamQuality newQuality = m_cameraRes->getSecondaryStreamQuality();
            if (newQuality != m_quality)
            {
                m_quality = newQuality;
                needUpdate = true;
            }

            int oldFps = m_fps;
            int newFps = qMin(m_cameraRes->desiredSecondStreamFps(), m_cameraRes->getMaxFps()); //ss; target for secind stream fps is 5 
            newFps = qMax(1, newFps);

            if (newFps != oldFps)
            {
                needUpdate = true;
            }

            m_fps = newFps;
        }

    }

    if (needUpdate)
        updateStreamParamsBasedOnQuality();
}

Qn::ConnectionRole QnLiveStreamProvider::getRole() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_role;
}

void QnLiveStreamProvider::setCameraControlDisabled(bool value)
{
    if (!value && m_prevCameraControlDisabled)
        updateStreamParamsBasedOnQuality();
    m_prevCameraControlDisabled = value;
}

void QnLiveStreamProvider::setSecondaryQuality(Qn::SecondStreamQuality  quality)
{
    {
        QMutexLocker mtx(&m_livemutex);
        if (m_secondaryQuality == quality)
            return; // same quality
        m_secondaryQuality = quality;
        if (m_secondaryQuality == Qn::SSQualityNotDefined)
            return;
    }

    if (getRole() != Qn::CR_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        if (m_owner) {
            QnLiveStreamProviderPtr lp = m_owner->getSecondaryReader();
            if (lp)
            {
                lp->setQuality(m_cameraRes->getSecondaryStreamQuality());
                lp->onPrimaryFpsUpdated(m_fps);
            }
        }
        updateStreamParamsBasedOnFps();
    }
}

void QnLiveStreamProvider::setQuality(Qn::StreamQuality q)
{
    {
        QMutexLocker mtx(&m_livemutex);
        if (m_quality == q && m_qualityUpdatedAtLeastOnce)
            return; // same quality

        m_quality = q;

        m_qualityUpdatedAtLeastOnce = true;

    }

    updateStreamParamsBasedOnQuality();
}

Qn::StreamQuality QnLiveStreamProvider::getQuality() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_quality;
}

Qn::ConnectionRole QnLiveStreamProvider::roleForMotionEstimation()
{
    QMutexLocker lock(&m_motionRoleMtx);

    if (m_softMotionRole == Qn::CR_Default) 
    {
        m_forcedMotionStream = m_cameraRes->getProperty(QnMediaResource::motionStreamKey()).toLower();
        if (m_forcedMotionStream == lit("primary"))
            m_softMotionRole = Qn::CR_LiveVideo;
        else if (m_forcedMotionStream == lit("secondary"))
            m_softMotionRole = Qn::CR_SecondaryLiveVideo;
        else {
            if (m_cameraRes && !m_cameraRes->hasDualStreaming2() && (m_cameraRes->getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability))
                m_softMotionRole = Qn::CR_LiveVideo;
            else
                m_softMotionRole = Qn::CR_SecondaryLiveVideo;
		}
    }
    return m_softMotionRole;
}

void QnLiveStreamProvider::onStreamResolutionChanged( int /*channelNumber*/, const QSize& /*picSize*/ )
{
}

void QnLiveStreamProvider::updateSoftwareMotion()
{
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid && getRole() == roleForMotionEstimation())
    {
        for (int i = 0; i < m_layout->channelCount(); ++i)
        {
            QnMotionRegion region = m_cameraRes->getMotionRegion(i);
            m_motionEstimation[i].setMotionMask(region);
        }
    }
#endif
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(m_cameraRes->getMotionMask(i), (char*)m_motionMaskBinData[i]);
}

bool QnLiveStreamProvider::canChangeStatus() const
{
    return m_role == Qn::CR_LiveVideo && m_isPhysicalResource;
}

// for live providers only
void QnLiveStreamProvider::setFps(float f)
{
    {
        QMutexLocker mtx(&m_livemutex);

        if (abs(m_fps - f) < 0.1)
            return; // same fps?


        m_fps = qMin((int)f, m_cameraRes->getMaxFps());
        f = m_fps;

    }
    
    if (getRole() != Qn::CR_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        if (m_owner) {
            QnLiveStreamProviderPtr lp = m_owner->getSecondaryReader();
            if (lp)
                lp->onPrimaryFpsUpdated(f);
        }
    }


    updateStreamParamsBasedOnFps();
}

float QnLiveStreamProvider::getFps() const
{
    QMutexLocker mtx(&m_livemutex);

    if (m_fps < 0) // setfps never been called
        m_fps = m_cameraRes->getMaxFps() - 2; // 2 here is out of the blue; just somthing not maximum

    m_fps = qMin((int)m_fps, m_cameraRes->getMaxFps());
    m_fps = qMax((int)m_fps, 1);

    return m_fps;
}

bool QnLiveStreamProvider::isMaxFps() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_fps >= m_cameraRes->getMaxFps()-0.1;
}

bool QnLiveStreamProvider::needMetaData() 
{
    // I assume this function is called once per video frame 

    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
    {
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        if (getRole() == roleForMotionEstimation()) {
            for (int i = 0; i < m_layout->channelCount(); ++i)
            {
                bool rez = m_motionEstimation[i].existsMetadata();
                if (rez) {
                    m_softMotionLastChannel = i;
                    return rez;
                }
            }
        }
#endif
        return false;
    }
    else if (getRole() == Qn::CR_LiveVideo && (m_cameraRes->getMotionType() == Qn::MT_HardwareGrid || m_cameraRes->getMotionType() == Qn::MT_MotionWindow))
    {
        bool result = (m_framesSinceLastMetaData > 10 ||
                       (m_framesSinceLastMetaData > 0 && m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS));
        if (result)
        {
            m_framesSinceLastMetaData = 0;
            m_timeSinceLastMetaData.restart();
        }
        return result;
    }
    
    return false; // not motion configured
}

static const int PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS = 10*1000;

void QnLiveStreamProvider::onGotVideoFrame(const QnCompressedVideoDataPtr& videoData)
{
    m_framesSinceLastMetaData++;

    saveMediaStreamParamsIfNeeded( videoData );

#ifdef ENABLE_SOFTWARE_MOTION_DETECTION

    int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();
    bool resoulutionOK =  videoData->width * videoData->height <= maxSquare || !m_forcedMotionStream.isEmpty();

    if (m_role == roleForMotionEstimation() && m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid && resoulutionOK)
    {
        if( m_motionEstimation[videoData->channelNumber].analizeFrame(videoData) )
            updateStreamResolution( videoData->channelNumber, m_motionEstimation[videoData->channelNumber].videoResolution() );
    }
    else if( !m_cameraRes->hasDualStreaming2() &&
             (m_role == Qn::CR_LiveVideo) &&                         //two conditions mean no motion at all
             (!m_resolutionCheckTimer.isValid() || m_resolutionCheckTimer.elapsed() > PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS) )    //from time to time checking primary stream resolution
    {
        QSize newResolution;
        extractMediaStreamParams( videoData, &newResolution );
        if( newResolution.isValid() )
        {
            updateStreamResolution( videoData->channelNumber, newResolution );
            m_resolutionCheckTimer.restart();
        }
    }
#endif
}

void QnLiveStreamProvider::onPrimaryFpsUpdated(int newFps)
{
    Q_ASSERT(getRole() == Qn::CR_SecondaryLiveVideo);
    // now primary has newFps
    // this is secondary stream
    // need to adjust fps 

    int maxFps = m_cameraRes->getMaxFps();

    Qn::StreamFpsSharingMethod sharingMethod = m_cameraRes->streamFpsSharingMethod();
    int newSecFps;

    if (secondaryResolutionIsLarge())
        newSecFps = MIN_SECOND_STREAM_FPS;
    else if (sharingMethod == Qn::PixelsFpsSharing)
    {
        newSecFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxFps); //minimum between DESIRED_SECOND_STREAM_FPS and what is left;
        if (maxFps - newFps < 2 )
            newSecFps = qMin(m_cameraRes->desiredSecondStreamFps()/2,MIN_SECOND_STREAM_FPS);

    }
    else if (sharingMethod == Qn::BasicFpsSharing)
        newSecFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxFps - newFps); //ss; minimum between 5 and what is left;
    else// noSharing
        newSecFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxFps);




    //Q_ASSERT(newSecFps>=0); // default fps is 10. Some camers has lower fps and assert is appear

    setFps(qMax(1,newSecFps));
}

QnMetaDataV1Ptr QnLiveStreamProvider::getMetaData()
{
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
        return m_motionEstimation[m_softMotionLastChannel].getMotion();
    else
#endif
        return getCameraMetadata();
}

QnMetaDataV1Ptr QnLiveStreamProvider::getCameraMetadata()
{
    QnMetaDataV1Ptr result(new QnMetaDataV1(1));
    result->m_duration = 1000*1000*1000; // 1000 sec 
    return result;
}

bool QnLiveStreamProvider::hasRunningLiveProvider(QnNetworkResource* netRes)
{
    bool rez = false;
    netRes->lockConsumers();
    for(QnResourceConsumer* consumer: netRes->getAllConsumers())
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (lp)
        {
            QnLongRunnable* lr = dynamic_cast<QnLongRunnable*>(lp);
            if (lr && lr->isRunning()) {
                rez = true;
                break;
            }
        }
    }

    netRes->unlockConsumers();
    return rez;
}

void QnLiveStreamProvider::startIfNotRunning()
{
    QMutexLocker mtx(&m_mutex);
    if (!isRunning())    
    {
        start();
    }
}

bool QnLiveStreamProvider::isCameraControlDisabled() const
{
    const QnVirtualCameraResource* camRes = dynamic_cast<const QnVirtualCameraResource*>(m_resource.data());
    return camRes && camRes->isCameraControlDisabled();
}

bool QnLiveStreamProvider::isCameraControlRequired() const
{
    return !isCameraControlDisabled() && needConfigureProvider();
}

void QnLiveStreamProvider::filterMotionByMask(const QnMetaDataV1Ptr& motion)
{
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
}

void QnLiveStreamProvider::updateStreamResolution( int channelNumber, const QSize& newResolution )
{
    if( newResolution == m_videoResolutionByChannelNumber[channelNumber] )
        return;

    m_videoResolutionByChannelNumber[channelNumber] = newResolution;
    onStreamResolutionChanged( channelNumber, newResolution );

    if( getRole() == Qn::CR_SecondaryLiveVideo ||
        m_cameraRes->hasCameraCapabilities( Qn::PrimaryStreamSoftMotionCapability ) || 
        m_cameraRes->hasDualStreaming2() )
    {
        return;
    }

    //no secondary stream and no motion, may be primary stream is now OK for motion?
    bool newValue = newResolution.width()*newResolution.height() <= MAX_PRIMARY_RES_FOR_SOFT_MOTION;
    bool cameraValue = m_cameraRes->getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability;
    if (newValue != cameraValue) 
    {
        if( newValue)
            m_cameraRes->setCameraCapability( Qn::PrimaryStreamSoftMotionCapability, true );
        else
            m_cameraRes->setCameraCapabilities( m_cameraRes->getCameraCapabilities() & ~Qn::PrimaryStreamSoftMotionCapability );
        m_cameraRes->saveParams();
    }

    m_softMotionRole = Qn::CR_Default;    //it will be auto-detected on the next frame
    QMutexLocker mtx(&m_livemutex);
    updateSoftwareMotion();
}

void QnLiveStreamProvider::updateSoftwareMotionStreamNum()
{
    QMutexLocker lock(&m_motionRoleMtx);
    m_softMotionRole = Qn::CR_Default;    //it will be auto-detected on the next frame
}

void QnLiveStreamProvider::extractMediaStreamParams(
    const QnCompressedVideoDataPtr& videoData,
    QSize* const newResolution,
    std::map<QString, QString>* const customStreamParams )
{
    switch( videoData->compressionType )
    {
        case CODEC_ID_H264:
            if( customStreamParams )
                extractSpsPps( videoData, customStreamParams );

        case CODEC_ID_MPEG2VIDEO:
            if( videoData->width > 0 && videoData->height > 0 )
                *newResolution = QSize( videoData->width, videoData->height );
            //TODO #ak it is very possible that videoData->width and videoData->height do not change when stream resolution changes and there is no SPS also
            break;

        case CODEC_ID_MJPEG:
        {
            nx_jpg::ImageInfo imgInfo;
            if( !nx_jpg::readJpegImageInfo( (const quint8*)videoData->data(), videoData->dataSize(), &imgInfo ) )
                return;
            *newResolution = QSize( imgInfo.width, imgInfo.height );
            break;
        }
        default:
            if( videoData->width > 0 && videoData->height > 0 )
                *newResolution = QSize( videoData->width, videoData->height );
            break;
    }
}

void QnLiveStreamProvider::saveMediaStreamParamsIfNeeded( const QnCompressedVideoDataPtr& videoData )
{
    ++m_framesSincePrevMediaStreamCheck;
    if( m_framesSincePrevMediaStreamCheck < CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES ||
        (videoData->flags & QnAbstractMediaData::MediaFlags_AVKey) == 0 )
        return;
    m_framesSincePrevMediaStreamCheck = 0;

    QSize streamResolution;
    //vector<pair<name, value>>
    std::map<QString, QString> customStreamParams;
    extractMediaStreamParams(
        videoData,
        &streamResolution,
        &customStreamParams );

    CameraMediaStreamInfo mediaStreamInfo(
        getRole() == Qn::CR_LiveVideo
            ? PRIMARY_ENCODER_INDEX
            : SECONDARY_ENCODER_INDEX,
        QSize(streamResolution.width(), streamResolution.height()),
        videoData->compressionType,
        std::move(customStreamParams) );

    if( m_cameraRes->saveMediaStreamInfoIfNeeded( mediaStreamInfo ) )
        m_cameraRes->saveParamsAsync();
}

namespace
{
    bool isH264SeqHeaderInExtraData( const QnConstCompressedVideoDataPtr data )
    {
        return data->context &&
            data->context->ctx() &&
            data->context->ctx()->extradata_size >= 7 &&
            data->context->ctx()->extradata[0] == 1;
    }

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

    void readH264NALUsFromExtraData(
        const QnConstCompressedVideoDataPtr data,
        std::vector<std::pair<const quint8*, size_t>>* const nalUnits )
    {
        const unsigned char* p = data->context->ctx()->extradata;

        //sps & pps is in the extradata, parsing it...
        //following code has been taken from libavcodec/h264.c

        // prefix is unit len
        //const int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;
        /* sps and pps in the avcC always have length coded with 2 bytes,
         * so put a fake nal_length_size = 2 while parsing them */
        //int nal_length_size = 2;

        // Decode sps from avcC
        int cnt = *(p + 5) & 0x1f; // Number of sps
        p += 6;

        for( int i = 0; i < cnt; i++ )
        {
            const int nalsize = AV_RB16(p);
            p += 2; //skipping nalusize
            if( nalsize > data->context->ctx()->extradata_size - (p - data->context->ctx()->extradata) )
                break;
            nalUnits->emplace_back( (const quint8*)p, nalsize );
            p += nalsize;
        }

        // Decode pps from avcC
        cnt = *(p++); // Number of pps
        for( int i = 0; i < cnt; ++i )
        {
            const int nalsize = AV_RB16(p);
            p += 2;
            if( nalsize > data->context->ctx()->extradata_size - (p - data->context->ctx()->extradata) )
                break;

            nalUnits->emplace_back( (const quint8*)p, nalsize );
            p += nalsize;
        }
    }

    void readH264NALUsFromAnnexBStream(
        const QnConstCompressedVideoDataPtr data,
        std::vector<std::pair<const quint8*, size_t>>* const nalUnits )
    {
        const quint8* dataStart = reinterpret_cast<const quint8*>(data->data());
        const quint8* dataEnd = dataStart + data->dataSize();
        const quint8* naluEnd = nullptr;
        for( const quint8
             *curNalu = NALUnit::findNALWithStartCodeEx( dataStart, dataEnd, &naluEnd ),
             *nextNalu = NULL;
             curNalu < dataEnd;
             curNalu = nextNalu )
        {
            nextNalu = NALUnit::findNALWithStartCodeEx( curNalu, dataEnd, &naluEnd );
            Q_ASSERT( nextNalu > curNalu );
            //skipping leading_zero_8bits and trailing_zero_8bits
            while( (naluEnd > curNalu) && (*(naluEnd-1) == 0) )
                --naluEnd;
            nalUnits->emplace_back( (const quint8*)curNalu, naluEnd-curNalu );
        }
    }
}

void QnLiveStreamProvider::extractSpsPps(
    const QnCompressedVideoDataPtr& videoData,
    std::map<QString, QString>* const customStreamParams )
{
    //vector<pair<nalu buf, nalu size>>
    std::vector<std::pair<const quint8*, size_t>> nalUnits;
    if( isH264SeqHeaderInExtraData(videoData) )
        readH264NALUsFromExtraData( videoData, &nalUnits );
    else
        readH264NALUsFromAnnexBStream( videoData, &nalUnits );

    //generating profile-level-id and sprop-parameter-sets as in rfc6184
    QByteArray profileLevelID;
    QByteArray spropParameterSets;

    for( const std::pair<const quint8*, size_t>& nalu: nalUnits )
    {
        switch( *nalu.first & 0x1f )
        {
            case nuSPS:
                if( nalu.second < 3 )
                    continue;   //invalid sps
                profileLevelID = QByteArray::fromRawData( (const char*)nalu.first, 3 ).toHex();
                spropParameterSets =
                    QByteArray::fromRawData( (const char*)nalu.first, nalu.second ).toBase64() +
                        "," + spropParameterSets;
                break;

            case nuPPS:
                spropParameterSets +=
                    QByteArray::fromRawData( (const char*)nalu.first, nalu.second ).toBase64();
                break;
        }
    }

    if( !profileLevelID.isEmpty() )
        customStreamParams->emplace( Qn::PROFILE_LEVEL_ID_PARAM_NAME, QLatin1String(profileLevelID) );
    if( !spropParameterSets.isEmpty() )
        customStreamParams->emplace( Qn::SPROP_PARAMETER_SETS_PARAM_NAME, QLatin1String(spropParameterSets) );
}

#endif // ENABLE_DATA_PROVIDERS
