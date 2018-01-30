#include "live_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <motion/motion_detection.h>

#include <nx/utils/log/log.h>
#include <nx/utils/safe_direct_connection.h>

#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>

#include <core/dataconsumer/conditional_data_proxy.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_properties.h>

#include <common/common_module.h>

#include <utils/media/h264_utils.h>
#include <utils/media/jpeg_utils.h>
#include <utils/media/nalUnits.h>
#include <utils/media/av_codec_helper.h>
#include <utils/common/synctime.h>
#include <camera/video_camera.h>
#include <mediaserver_ini.h>
#include <analytics/detected_objects_storage/analytics_events_receptor.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/metadata/manager_pool.h>
#include <nx/fusion/model_functions.h>

static const int CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES = 1000;
static const int PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS = 10 * 1000;
static const int SAVE_BITRATE_FRAME = 300; // value TBD

class QnLiveStreamProvider::MetadataDataReceptor: public QnAbstractDataReceptor
{
public:
    MetadataDataReceptor()
    {
    }

    virtual bool canAcceptData() const override
    {
        return true;
    }

    virtual void putData(const QnAbstractDataPacketPtr& data) override
    {
        const QnAbstractCompressedMetadataPtr& metadata =
            std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(data);
        if (metadata)
            metadataQueue.push(metadata);
    }
public:
    QnSafeQueue<QnAbstractCompressedMetadataPtr> metadataQueue;
};

QnLiveStreamProvider::QnLiveStreamProvider(const QnResourcePtr& res):
    QnAbstractMediaStreamDataProvider(res),
    m_liveMutex(QnMutex::Recursive),
    m_framesSinceLastMetaData(0),
    m_totalVideoFrames(0),
    m_totalAudioFrames(0),
    m_softMotionRole(Qn::CR_Default),
    m_softMotionLastChannel(0),
    m_videoChannels(1),
    m_framesSincePrevMediaStreamCheck(CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES+1),
    m_metadataReceptor(new MetadataDataReceptor())
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        m_motionMaskBinData[i] =
            (simd128i*) qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight/8, 32);
        memset(m_motionMaskBinData[i], 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight/8);
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        m_motionEstimation[i].setChannelNum(i);
#endif
    }

    m_role = Qn::CR_LiveVideo;
    m_timeSinceLastMetaData.restart();
    m_cameraRes = res.dynamicCast<QnVirtualCameraResource>();
    NX_CRITICAL(m_cameraRes && m_cameraRes->flags().testFlag(Qn::local_live_cam));
    m_prevCameraControlDisabled = m_cameraRes->isCameraControlDisabled();
    m_videoChannels = m_cameraRes->getVideoLayout()->channelCount();
    m_resolutionCheckTimer.invalidate();

    Qn::directConnect(res.data(), &QnResource::videoLayoutChanged, this, [this](const QnResourcePtr&) {
        m_videoChannels = m_cameraRes->getVideoLayout()->channelCount();
        QnMutexLocker mtx( &m_liveMutex );
        updateSoftwareMotion();
    });

    m_dataReceptorMultiplexer.reset(new DataCopier());
    m_dataReceptorMultiplexer->add(m_metadataReceptor);

    // Forwarding metadata to analytics events DB.
    if (qnServerModule)
    {
        m_analyticsEventsSaver = QnAbstractDataReceptorPtr(
            new nx::analytics::storage::AnalyticsEventsReceptor(
                qnServerModule->analyticsEventsStorage()));
        m_analyticsEventsSaver = QnAbstractDataReceptorPtr(
            new ConditionalDataProxy(
                m_analyticsEventsSaver,
                [this](const QnAbstractDataPacketPtr& /*data*/)
                {
                    return m_cameraRes->getStatus() == Qn::Recording;
                }));
        m_dataReceptorMultiplexer->add(m_analyticsEventsSaver);
        auto pool = qnServerModule->metadataManagerPool();
        pool->registerDataReceptor(getResource(), m_dataReceptorMultiplexer.toWeakRef());
    }
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{
    directDisconnectAll();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        qFreeAligned(m_motionMaskBinData[i]);
}

void QnLiveStreamProvider::setOwner(QnSharedResourcePointer<QnAbstractVideoCamera> owner)
{
    m_owner = owner.toWeakRef();
}

QnSharedResourcePointer<QnAbstractVideoCamera> QnLiveStreamProvider::getOwner() const
{
    return m_owner.toStrongRef();
}

void QnLiveStreamProvider::setRole(Qn::ConnectionRole role)
{
    QnAbstractMediaStreamDataProvider::setRole(role);
    QnMutexLocker mtx(&m_liveMutex);
    updateSoftwareMotion();

    const auto roleForAnalytics = ini().analyzeSecondaryStream
        ? Qn::ConnectionRole::CR_SecondaryLiveVideo
        : Qn::ConnectionRole::CR_LiveVideo;

    if (role == roleForAnalytics && qnServerModule)
    {
        auto pool = qnServerModule->metadataManagerPool();
        m_videoDataReceptor = pool->mediaDataReceptor(m_cameraRes->getId());
    }
}

Qn::ConnectionRole QnLiveStreamProvider::getRole() const
{
    QnMutexLocker mtx( &m_liveMutex );
    return m_role;
}

Qn::StreamIndex QnLiveStreamProvider::encoderIndex() const
{
    return getRole() == Qn::CR_LiveVideo ? Qn::StreamIndex::primary
                                         : Qn::StreamIndex::secondary;
}

void QnLiveStreamProvider::setCameraControlDisabled(bool value)
{
    if (!value && m_prevCameraControlDisabled)
        pleaseReopenStream();
    m_prevCameraControlDisabled = value;
}

QnLiveStreamParams QnLiveStreamProvider::mergeWithAdvancedParams(const QnLiveStreamParams& value)
{
    QnLiveStreamParams params = value;
    if (params.bitrateKbps == 0 && params.quality == Qn::QualityNotDefined)
        params.quality = Qn::QualityNormal;

    // Add advanced parameters
    const auto advancedParams = m_cameraRes->advancedLiveStreamParams();
    auto advancedLiveStreamParams = getRole() == Qn::CR_SecondaryLiveVideo
        ? advancedParams.secondaryStream : advancedParams.primaryStream;

    if (params.fps == QnLiveStreamParams::kFpsNotInitialized)
    {
        params.fps = advancedLiveStreamParams.fps;
        if (params.fps == QnLiveStreamParams::kFpsNotInitialized)
            params.fps = getDefaultFps();
    }
    params.fps = qBound(
        (float) 1.0, params.fps, (float)m_cameraRes->getMaxFps());

    if (params.resolution.isEmpty())
        params.resolution = advancedLiveStreamParams.resolution;
    if (params.codec.isEmpty())
        params.codec = advancedLiveStreamParams.codec;
    if (m_role == Qn::CR_SecondaryLiveVideo)
        params.bitrateKbps = advancedLiveStreamParams.bitrateKbps;
    if (params.bitrateKbps == 0)
        params.bitrateKbps = m_cameraRes->rawSuggestBitrateKbps(params.quality, params.resolution, params.fps);
    return params;
}

void QnLiveStreamProvider::setPrimaryStreamParams(const QnLiveStreamParams& params)
{
    QnMutexLocker lock(&m_mutex);
    if (m_liveParams == params)
        return;
    m_liveParams = params;

    if (getRole() != Qn::CR_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        if (auto owner = getOwner().dynamicCast<QnAbstractMediaServerVideoCamera>())
        {
            QnLiveStreamProviderPtr lp = owner->getSecondaryReader();
            if (lp)
                lp->onPrimaryFpsChanged(params.fps);
        }
    }

    pleaseReopenStream();
}

Qn::ConnectionRole QnLiveStreamProvider::roleForMotionEstimation()
{
    QnMutexLocker lock( &m_motionRoleMtx );

    if (m_softMotionRole == Qn::CR_Default)
    {
        m_forcedMotionStream = m_cameraRes->getProperty(QnMediaResource::motionStreamKey()).toLower();
        if (m_forcedMotionStream == QnMediaResource::primaryStreamValue())
            m_softMotionRole = Qn::CR_LiveVideo;
        else if (m_forcedMotionStream == QnMediaResource::secondaryStreamValue())
            m_softMotionRole = Qn::CR_SecondaryLiveVideo;
        else
        {
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
        for (int i = 0; i < m_videoChannels; ++i)
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
    return m_role == Qn::CR_LiveVideo;
}

float QnLiveStreamProvider::getDefaultFps() const
{
    float maxFps = m_cameraRes->getMaxFps();
    int reservedSecondStreamFps = m_cameraRes->reservedSecondStreamFps();

    return qMax(1.0, maxFps - reservedSecondStreamFps);
}

bool QnLiveStreamProvider::needAnalyzeMotion(Qn::ConnectionRole /*role*/)
{
    return m_role == roleForMotionEstimation()
        && m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid;
}

bool QnLiveStreamProvider::isMaxFps() const
{
    QnMutexLocker mtx( &m_liveMutex );
    return m_liveParams.fps >= m_cameraRes->getMaxFps() - 0.1;
}

bool QnLiveStreamProvider::needMetadata()
{
    // I assume this function is called once per video frame
    if (!m_metadataReceptor->metadataQueue.isEmpty())
        return true;

    bool needHardwareMotion = getRole() == Qn::CR_LiveVideo
        && (m_cameraRes->getMotionType() == Qn::MT_HardwareGrid
            || m_cameraRes->getMotionType() == Qn::MT_MotionWindow);

    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
    {
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        if (needAnalyzeMotion(getRole()))
        {
            for (int i = 0; i < m_videoChannels; ++i)
            {
                bool rez = m_motionEstimation[i].existsMetadata();
                if (rez)
                {
                    m_softMotionLastChannel = i;
                    return rez;
                }
            }
        }
#endif
        return false;
    }
    else if (needHardwareMotion)
    {
        bool result = m_framesSinceLastMetaData > 10
            || (m_framesSinceLastMetaData > 0
                && m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS);

        if (result)
        {
            m_framesSinceLastMetaData = 0;
            m_timeSinceLastMetaData.restart();
        }
        return result;
    }

    return false; //< Motion is not configured
}

void QnLiveStreamProvider::onStreamReopen()
{
    m_totalVideoFrames = 0;
}

void QnLiveStreamProvider::onGotVideoFrame(
    const QnCompressedVideoDataPtr& videoData,
    const QnLiveStreamParams& currentLiveParams,
    bool isCameraControlRequired)
{
    m_totalVideoFrames++;
    m_framesSinceLastMetaData++;

    saveMediaStreamParamsIfNeeded(videoData);
    if (m_totalVideoFrames && (m_totalVideoFrames % SAVE_BITRATE_FRAME) == 0)
        saveBitrateIfNeeded(videoData, currentLiveParams, isCameraControlRequired);

#if defined(ENABLE_SOFTWARE_MOTION_DETECTION)

    bool updateResolutionFromPrimaryStream = !m_cameraRes->hasDualStreaming2()
        && (m_role == Qn::CR_LiveVideo)
        && (!m_resolutionCheckTimer.isValid()
            || m_resolutionCheckTimer.elapsed() > PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS);

    if (needAnalyzeMotion(getRole()))
    {
        static const int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()
            * SECONDARY_STREAM_MAX_RESOLUTION.height();

        const bool needAnalyzeFrame = !m_forcedMotionStream.isEmpty()
            || videoData->width * videoData->height <= maxSquare;

        const int channel = videoData->channelNumber;

        if (needAnalyzeFrame && m_motionEstimation[channel].analyzeFrame(videoData))
        {
            updateStreamResolution(
                channel,
                m_motionEstimation[channel].videoResolution());
        }
    }
    else if (updateResolutionFromPrimaryStream)
    {
        QSize newResolution;
        extractMediaStreamParams(videoData, &newResolution);
        if (newResolution.isValid())
        {
            updateStreamResolution( videoData->channelNumber, newResolution );
            m_resolutionCheckTimer.restart();
        }
    }

    if (ini().enableMetadataProcessing)
    {
        const bool needToAnalyzeFrame = !ini().analyzeKeyFramesOnly
            || videoData->flags & QnAbstractMediaData::MediaFlags_AVKey;

        const auto roleForAnalytics = ini().analyzeSecondaryStream
            ? Qn::ConnectionRole::CR_SecondaryLiveVideo
            : Qn::ConnectionRole::CR_LiveVideo;

        const bool needToAnalyzeStream = roleForAnalytics == getRole();

        if (needToAnalyzeFrame && needToAnalyzeStream)
        {
            auto receptor = m_videoDataReceptor.toStrongRef();
            if (receptor && receptor->canAcceptData())
            {
                NX_VERBOSE(this) << "Pushing to receptor, timestamp:" << videoData->timestamp;
                receptor->putData(videoData);
            }
        }
    }
#endif // ENABLE_SOFTWARE_MOTION_DETECTION
}

void QnLiveStreamProvider::onGotAudioFrame(const QnCompressedAudioDataPtr& audioData)
{
    if (m_totalAudioFrames++ == 0 &&    // only once
        getRole() == Qn::CR_LiveVideo) // only primary stream
    {
        // save only once
        const auto savedCodec = m_cameraRes->getProperty(Qn::CAMERA_AUDIO_CODEC_PARAM_NAME);
        const QString actualCodec = QnAvCodecHelper::codecIdToString(audioData->compressionType);
        if (savedCodec.isEmpty())
        {
            m_cameraRes->setProperty(Qn::CAMERA_AUDIO_CODEC_PARAM_NAME, actualCodec);
            m_cameraRes->saveParams();
        }
    }
}

void QnLiveStreamProvider::onPrimaryFpsChanged(int primaryFps)
{
    NX_ASSERT(getRole() == Qn::CR_SecondaryLiveVideo);
    // now primary has newFps
    // this is secondary stream
    // need to adjust fps

    QnLiveStreamParams params;
    const Qn::StreamFpsSharingMethod sharingMethod = m_cameraRes->streamFpsSharingMethod();
    int newSecondaryStreamFps = m_cameraRes->defaultSecondaryFps(params.quality);

    if (sharingMethod == Qn::PixelsFpsSharing)
    {
        if (m_cameraRes->getMaxFps() - primaryFps < 2 )
            newSecondaryStreamFps /= 2;
    }
    else if (sharingMethod == Qn::BasicFpsSharing)
    {
        newSecondaryStreamFps = qMin(
            newSecondaryStreamFps,
            m_cameraRes->getMaxFps() - primaryFps);
    }
    else
    {
        // noSharing
    }
    m_liveParams.fps = qBound(
        QnLiveStreamParams::kMinSecondStreamFps,
        newSecondaryStreamFps,
        m_cameraRes->getMaxFps());
}

QnLiveStreamParams QnLiveStreamProvider::getLiveParams()
{
    QnMutexLocker lock(&m_liveMutex);
    return mergeWithAdvancedParams(m_liveParams);
}

QnAbstractCompressedMetadataPtr QnLiveStreamProvider::getMetadata()
{
    if (!m_metadataReceptor->metadataQueue.isEmpty())
    {
        QnAbstractCompressedMetadataPtr metadata;
        m_metadataReceptor->metadataQueue.pop(metadata);
        return metadata;
    }

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
    result->m_duration = 1000 * 1000 * 1000; // 1000 sec
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
    QnMutexLocker mtx( &m_mutex );
    if (!isRunning())
    {
        m_framesSincePrevMediaStreamCheck = CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES+1;
        start();
    }
}

bool QnLiveStreamProvider::isCameraControlDisabled() const
{
    const QnVirtualCameraResource* camRes = dynamic_cast<const QnVirtualCameraResource*>(m_resource.data());
    return camRes && camRes->isCameraControlDisabled();
}

void QnLiveStreamProvider::filterMotionByMask(const QnMetaDataV1Ptr& motion)
{
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
}

void QnLiveStreamProvider::updateStreamResolution(int channelNumber, const QSize& newResolution)
{
    if( newResolution == m_videoResolutionByChannelNumber[channelNumber] )
        return;

    m_videoResolutionByChannelNumber[channelNumber] = newResolution;
    onStreamResolutionChanged( channelNumber, newResolution );

    if( getRole() == Qn::CR_SecondaryLiveVideo)
        return;

    //no secondary stream and no motion, may be primary stream is now OK for motion?
    bool newValue = newResolution.width()*newResolution.height() <= MAX_PRIMARY_RES_FOR_SOFT_MOTION
        || m_cameraRes->getProperty(QnMediaResource::motionStreamKey()) == QnMediaResource::primaryStreamValue();

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
    QnMutexLocker mtx( &m_liveMutex );
    updateSoftwareMotion();
}

void QnLiveStreamProvider::updateSoftwareMotionStreamNum()
{
    QnMutexLocker lock( &m_motionRoleMtx );
    m_softMotionRole = Qn::CR_Default;    //it will be auto-detected on the next frame
}

void QnLiveStreamProvider::extractMediaStreamParams(
    const QnCompressedVideoDataPtr& videoData,
    QSize* const newResolution,
    std::map<QString, QString>* const customStreamParams)
{
    switch( videoData->compressionType )
    {
        case AV_CODEC_ID_H264:
            extractSpsPps(
                videoData,
                (videoData->width > 0 && videoData->height > 0)
                    ? nullptr   //taking resolution from sps only if video frame does not already contain it
                    : newResolution,
                customStreamParams );

        case AV_CODEC_ID_MPEG2VIDEO:
            if( videoData->width > 0 && videoData->height > 0 )
                *newResolution = QSize( videoData->width, videoData->height );
            //TODO #ak it is very possible that videoData->width and videoData->height do not change when stream resolution changes and there is no SPS also
            break;

        case AV_CODEC_ID_MJPEG:
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

void QnLiveStreamProvider::saveMediaStreamParamsIfNeeded(const QnCompressedVideoDataPtr& videoData)
{
    ++m_framesSincePrevMediaStreamCheck;
    if( m_framesSincePrevMediaStreamCheck < CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES ||
        (videoData->flags & QnAbstractMediaData::MediaFlags_AVKey) == 0 )
        return;
    m_framesSincePrevMediaStreamCheck = 0;

    QSize streamResolution;
    std::map<QString, QString> customStreamParams;
    extractMediaStreamParams(
        videoData,
        &streamResolution,
        &customStreamParams );

    CameraMediaStreamInfo mediaStreamInfo(
        encoderIndex(),
        QSize(streamResolution.width(), streamResolution.height()),
        videoData->compressionType,
        std::move(customStreamParams) );

    if( m_cameraRes->saveMediaStreamInfoIfNeeded( mediaStreamInfo ) )
        m_cameraRes->saveParamsAsync();
}

void QnLiveStreamProvider::saveBitrateIfNeeded(
    const QnCompressedVideoDataPtr& videoData,
    const QnLiveStreamParams& liveParams,
    bool isCameraConfigured)
{
    auto now = qnSyncTime->currentDateTime().toUTC().toString(Qt::ISODate);
    CameraBitrateInfo info(encoderIndex(), std::move(now));

    info.rawSuggestedBitrate = m_cameraRes->rawSuggestBitrateKbps(
        liveParams.quality, liveParams.resolution, liveParams.fps) / 1024;
    info.suggestedBitrate = static_cast<float>(m_cameraRes->suggestBitrateKbps(
        liveParams, getRole())) / 1024;
    info.actualBitrate = getBitrateMbps() / getNumberOfChannels();

    info.bitratePerGop = m_cameraRes->bitratePerGopType();
    info.bitrateFactor = 1; // TODO: #mux Pass actual value when avaliable [2.6]
    info.numberOfChannels = getNumberOfChannels();

    info.fps = liveParams.fps;
    info.actualFps = getFrameRate();
    info.averageGopSize = getAverageGopSize();
    info.resolution = CameraMediaStreamInfo::resolutionToString(liveParams.resolution);
    info.isConfigured = isCameraConfigured;

    if (m_cameraRes->saveBitrateIfNeeded(info))
    {
        m_cameraRes->saveParamsAsync();
        NX_LOG(lm("QnLiveStreamProvider: bitrateInfo has been updated for %1 stream")
                .arg(QnLexical::serialized(info.encoderIndex)), cl_logINFO);
    }
}

#endif // ENABLE_DATA_PROVIDERS
