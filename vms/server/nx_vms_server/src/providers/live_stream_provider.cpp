#include "live_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <motion/motion_detection.h>

#include <nx/utils/log/log.h>
#include <nx/utils/safe_direct_connection.h>

#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/uncompressed_video_packet.h>
#include <nx/streaming/in_stream_compressed_metadata.h>

#include <core/dataconsumer/conditional_data_proxy.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_properties.h>

#include <common/common_module.h>

#include <utils/media/h264_utils.h>
#include <utils/media/jpeg_utils.h>
#include <utils/media/nalUnits.h>
#include <utils/media/av_codec_helper.h>
#include <utils/common/synctime.h>
#include <nx/streaming/config.h>
#include <utils/media/hevc_sps.h>
#include <camera/video_camera.h>
#include <nx_vms_server_ini.h>
#include <nx/vms/server/analytics/db/analytics_events_receptor.h>
#include <media_server/media_server_module.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/server/resource/camera.h>
#include <utils/media/utils.h>

#include <nx/analytics/analytics_logging_ini.h>

using namespace nx::vms::server::analytics;

static const int CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES = 1000;
static const int PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS = 10 * 1000;
static const int SAVE_BITRATE_FRAME = 300; // value TBD

using StreamType = nx::vms::api::analytics::StreamType;
using StreamTypes = nx::vms::api::analytics::StreamTypes;

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

QnLiveStreamProvider::QnLiveStreamProvider(const nx::vms::server::resource::CameraPtr& res)
    :
    QnAbstractMediaStreamDataProvider(res),
    nx::vms::server::ServerModuleAware(res->serverModule()),
    m_liveMutex(QnMutex::Recursive),
    m_framesSinceLastMetaData(0),
    m_totalVideoFrames(0),
    m_totalAudioFrames(0),
    m_softMotionLastChannel(0),
    m_videoChannels(1),
    m_framesSincePrevMediaStreamCheck(CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES+1),
    m_metadataReceptor(new MetadataDataReceptor())
{
    QnMotionEstimation::Config config;
    if (serverModule())
        config.decoderConfig.mtDecodePolicy = serverModule()->settings().multiThreadDecodePolicy();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        m_motionEstimation.emplace_back(
            std::make_unique<QnMotionEstimation>(config, res->commonModule()->metrics()));
        m_motionMaskBinData[i] =
            (simd128i*) qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight/8, 32);
        memset(m_motionMaskBinData[i], 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight/8);
        m_motionEstimation[i]->setChannelNum(i);
    }

    QnAbstractStreamDataProvider::setRole(Qn::CR_LiveVideo);
    m_timeSinceLastMetaData.restart();
    m_cameraRes = res.dynamicCast<QnVirtualCameraResource>();
    NX_CRITICAL(m_cameraRes && m_cameraRes->flags().testFlag(Qn::local_live_cam));
    m_prevCameraControlDisabled = m_cameraRes->isCameraControlDisabled();
    m_videoChannels = std::min(CL_MAX_CHANNELS, m_cameraRes->getVideoLayout()->channelCount());
    m_resolutionCheckTimer.invalidate();

    Qn::directConnect(res.data(), &QnResource::videoLayoutChanged, this, [this](const QnResourcePtr&) {
        m_videoChannels = std::min(CL_MAX_CHANNELS, m_cameraRes->getVideoLayout()->channelCount());
        QnMutexLocker lock(&m_liveMutex);
        updateSoftwareMotion();
    });

    m_dataReceptorMultiplexer.reset(new DataCopier());
    m_dataReceptorMultiplexer->add(m_metadataReceptor);

    // Forwarding metadata to analytics events DB.
    if (serverModule())
    {
        m_analyticsEventsSaver = QnAbstractDataReceptorPtr(
            new nx::analytics::db::AnalyticsEventsReceptor(
                serverModule()->commonModule(),
                serverModule()->analyticsEventsStorage()));
        m_analyticsEventsSaver = QnAbstractDataReceptorPtr(
            new ConditionalDataProxy(
                m_analyticsEventsSaver,
                [deviceWeakPtr = m_cameraRes.toWeakRef()](const QnAbstractDataPacketPtr& /*data*/)
                {
                    // TODO: Weak pointer solution is temporary. Live stream provider should
                    // properly unsubscribe from metadata source.
                    if (const auto device = deviceWeakPtr.toStrongRef())
                        return device->getStatus() == Qn::Recording;

                    return false;
                }));
        m_dataReceptorMultiplexer->add(m_analyticsEventsSaver);
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
    updateSoftwareMotion();

    if (NX_ASSERT(serverModule()))
    {
        m_streamDataReceptor = serverModule()->analyticsManager()->registerMediaSource(
            m_cameraRes->getId(), Qn::toStreamIndex(getRole()));

        if (getRole() == Qn::ConnectionRole::CR_LiveVideo)
        {
            serverModule()->analyticsManager()->registerMetadataSink(
                getResource(), m_dataReceptorMultiplexer.toWeakRef());
        }
    }

    if (nx::analytics::loggingIni().isLoggingEnabled() && !m_metadataLogger)
    {
        m_metadataLogger = std::make_unique<nx::analytics::MetadataLogger>(
            "live_stream_provider_",
            m_cameraRes->getId(),
            /*engineId*/ QnUuid(),
            role == Qn::ConnectionRole::CR_LiveVideo
                ? nx::vms::api::StreamIndex::primary
                : nx::vms::api::StreamIndex::secondary);
    }
}

nx::vms::api::StreamIndex QnLiveStreamProvider::encoderIndex() const
{
    return QnSecurityCamResource::toStreamIndex(getRole());
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

    // Add advanced parameters.
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
    if (getRole() == Qn::CR_SecondaryLiveVideo)
        params.bitrateKbps = advancedLiveStreamParams.bitrateKbps;

    if (params.bitrateKbps == 0)
    {
        const bool isSecondary = getRole() == Qn::CR_SecondaryLiveVideo;
        if (params.quality == Qn::StreamQuality::undefined)
            params.quality = isSecondary ? Qn::StreamQuality::low : Qn::StreamQuality::normal;

        if (!params.resolution.isEmpty())
        {
            params.bitrateKbps = m_cameraRes->suggestBitrateForQualityKbps(
                params.quality, params.resolution, params.fps, params.codec, getRole());
        }
    }

    return params;
}

void QnLiveStreamProvider::setPrimaryStreamParams(const QnLiveStreamParams& params)
{
    {
        QnMutexLocker lock(&m_liveMutex);
        if (m_liveParams == params)
            return;
        m_liveParams = params;
    }

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

void QnLiveStreamProvider::onStreamResolutionChanged( int /*channelNumber*/, const QSize& /*picSize*/ )
{
}

void QnLiveStreamProvider::updateSoftwareMotion()
{
    for (int i = 0; i < m_videoChannels; ++i)
    {
        QnMotionRegion region = m_cameraRes->getMotionRegion(i);
        m_motionEstimation[i]->setMotionMask(region);
    }

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(m_cameraRes->getMotionMask(i), (char*)m_motionMaskBinData[i]);
}

float QnLiveStreamProvider::getDefaultFps() const
{
    float maxFps = m_cameraRes->getMaxFps();
    if (getRole() != Qn::CR_SecondaryLiveVideo)
    {
        int reservedSecondStreamFps = m_cameraRes->reservedSecondStreamFps();
        return qMax(1.0, maxFps - reservedSecondStreamFps);
    }

    // now primary has newFps
    // this is secondary stream
    // need to adjust fps

    QnLiveStreamParams params;
    const Qn::StreamFpsSharingMethod sharingMethod = m_cameraRes->streamFpsSharingMethod();
    int newSecondaryStreamFps = m_cameraRes->defaultSecondaryFps(params.quality);

    if (sharingMethod == Qn::PixelsFpsSharing)
    {
        if (m_cameraRes->getMaxFps() - m_primaryFps < 2)
            newSecondaryStreamFps /= 2;
    }
    else if (sharingMethod == Qn::BasicFpsSharing)
    {
        newSecondaryStreamFps = qMin(
            newSecondaryStreamFps,
            m_cameraRes->getMaxFps() - m_primaryFps);
    }
    else
    {
        // noSharing
    }
    return qBound(
        QnLiveStreamParams::kMinSecondStreamFps, newSecondaryStreamFps, m_cameraRes->getMaxFps());
}

bool QnLiveStreamProvider::doesStreamSuitMotionAnalysisRequirements()
{
    if (m_cameraRes->getMotionType() != Qn::MotionType::MT_SoftwareGrid)
        return false;

    const auto motionStreamIndex = m_cameraRes->motionStreamIndex().index;
    return motionStreamIndex == toStreamIndex(getRole());
}

bool QnLiveStreamProvider::isMaxFps() const
{
    QnMutexLocker lock(&m_liveMutex);
    return m_liveParams.fps >= m_cameraRes->getMaxFps() - 0.1;
}

bool QnLiveStreamProvider::needHardwareMotion()
{
    return getRole() == Qn::CR_LiveVideo
        && (m_cameraRes->getMotionType() == Qn::MotionType::MT_HardwareGrid
            || m_cameraRes->getMotionType() == Qn::MotionType::MT_MotionWindow);
}

bool QnLiveStreamProvider::needMetadata()
{
    // I assume this function is called once per video frame
    if (!m_metadataReceptor->metadataQueue.isEmpty())
        return true;

    if (m_cameraRes->getMotionType() == Qn::MotionType::MT_SoftwareGrid)
    {
        StreamProviderRequirements requirements;
        if (const auto streamReceptor = m_streamDataReceptor.toStrongRef())
            requirements = streamReceptor->providerRequirements(Qn::toStreamIndex(getRole()));

        if ((doesStreamSuitMotionAnalysisRequirements()
            && requirements.motionAnalysisPolicy == MotionAnalysisPolicy::automatic)
            || (requirements.motionAnalysisPolicy == MotionAnalysisPolicy::forced))
        {
            for (int i = 0; i < m_videoChannels; ++i)
            {
                if (m_lastMotionMetadata)
                {
                    m_softMotionLastChannel = i;
                    return true;
                }
            }
        }
        return false;
    }
    else if (needHardwareMotion())
    {
        bool result = m_framesSinceLastMetaData > META_FRAME_INTERVAL
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
    m_framesSincePrevMediaStreamCheck = CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES;
}

void QnLiveStreamProvider::onGotVideoFrame(
    const QnCompressedVideoDataPtr& compressedFrame,
    const QnLiveStreamParams& currentLiveParams,
    bool isCameraControlRequired)
{
    if (!NX_ASSERT(compressedFrame))
        return;

    m_totalVideoFrames++;
    m_framesSinceLastMetaData++;

    saveMediaStreamParamsIfNeeded(compressedFrame);
    if (m_totalVideoFrames && (m_totalVideoFrames % SAVE_BITRATE_FRAME) == 0)
        saveBitrateIfNeeded(compressedFrame, currentLiveParams, isCameraControlRequired);

    processMetadata(compressedFrame);
}

void QnLiveStreamProvider::processMetadata(
    const QnCompressedVideoDataPtr& compressedFrame)
{
    NX_VERBOSE(this) << lm("Proceeding with motion detection and/or feeding metadata plugins");

    bool needToAnalyzeMotion = false;
    if (doesStreamSuitMotionAnalysisRequirements())
        needToAnalyzeMotion = doesFrameSuitMotionAnalysisRequirements(compressedFrame);
    else
        checkAndUpdatePrimaryStreamResolution(compressedFrame);

    const auto streamDataReceptor = m_streamDataReceptor.toStrongRef();
    StreamProviderRequirements requirements;
    if (streamDataReceptor)
        requirements = streamDataReceptor->providerRequirements(Qn::toStreamIndex(getRole()));

    NX_ASSERT((requirements.motionAnalysisPolicy == MotionAnalysisPolicy::forced
        && requirements.requiredStreamTypes.testFlag(StreamType::motion))
        || (requirements.motionAnalysisPolicy == MotionAnalysisPolicy::disabled
        && !requirements.requiredStreamTypes.testFlag(StreamType::motion))
        || requirements.motionAnalysisPolicy == MotionAnalysisPolicy::automatic);

    if (requirements.motionAnalysisPolicy != MotionAnalysisPolicy::automatic)
        needToAnalyzeMotion = requirements.motionAnalysisPolicy == MotionAnalysisPolicy::forced;

    const int channel = compressedFrame->channelNumber;
    auto& motionEstimation = m_motionEstimation[channel];

    CLConstVideoDecoderOutputPtr uncompressedFrame;
    if (needToAnalyzeMotion)
    {
        NX_VERBOSE(this, "Analyzing motion (Role: [%1]); needUncompressedFrame: %2",
            getRole(),
            requirements.requiredStreamTypes.testFlag(StreamType::uncompressedVideo));

        if (motionEstimation->analyzeFrame(
                compressedFrame,
                requirements.requiredStreamTypes.testFlag(StreamType::uncompressedVideo)
                    ? &uncompressedFrame
                    : nullptr))
        {
            if (motionEstimation->tryToCreateMotionMetadata())
                m_lastMotionMetadata = motionEstimation->getMotion();

            updateStreamResolution(channel, motionEstimation->videoResolution());
        }
    }
    else if (requirements.requiredStreamTypes.testFlag(StreamType::uncompressedVideo))
    {
        NX_VERBOSE(this) << lm("Decoding frame for metadata plugins");
        uncompressedFrame = motionEstimation->decodeFrame(compressedFrame);
        if (!uncompressedFrame)
        {
            // TODO: #dmishin this logic seems suspicious, investigate it.
            NX_DEBUG(this,
                "Unable to get decoded frame for metadata plugins, compressed frame timestamp: %1",
                compressedFrame->timestamp);
        }
    }

    if (streamDataReceptor)
    {
        if (m_lastMotionMetadata && requirements.requiredStreamTypes.testFlag(StreamType::motion))
        {
            NX_VERBOSE(this, "Pushing motion metadata to receptor, timestamp: %1 us",
                m_lastMotionMetadata->timestamp);

            streamDataReceptor->putData(m_lastMotionMetadata);
        }

        if (requirements.requiredStreamTypes.testFlag(StreamType::uncompressedVideo)
            && uncompressedFrame)
        {
            NX_VERBOSE(this, "Pushing uncompressed frame to receptor, timestamp: %1 us",
                compressedFrame->timestamp);

            streamDataReceptor->putData(
                std::make_shared<nx::streaming::UncompressedVideoPacket>(uncompressedFrame));
        }

        if (requirements.requiredStreamTypes.testFlag(StreamType::compressedVideo))
        {
            NX_VERBOSE(this, "Pushing compressed frame to receptor, timestamp: %1 us",
                compressedFrame->timestamp);
            streamDataReceptor->putData(compressedFrame);
        }
    }
}

void QnLiveStreamProvider::afterRun()
{
    QnAbstractMediaStreamDataProvider::afterRun();
    for (auto& motionEstimation: m_motionEstimation)
        motionEstimation->stop();
}

void QnLiveStreamProvider::onGotAudioFrame(const QnCompressedAudioDataPtr& audioData)
{
    if (m_totalAudioFrames++ == 0 &&    // only once
        getRole() == Qn::CR_LiveVideo) // only primary stream
    {
        // save only once
        const auto savedCodec = m_cameraRes->getProperty(ResourcePropertyKey::kAudioCodec);
        const QString actualCodec = QnAvCodecHelper::codecIdToString(audioData->compressionType);
        if (savedCodec.isEmpty())
        {
            m_cameraRes->setProperty(ResourcePropertyKey::kAudioCodec, actualCodec);
            m_cameraRes->saveProperties();
        }
    }
}

void QnLiveStreamProvider::onGotInStreamMetadata(
    const std::shared_ptr<nx::streaming::InStreamCompressedMetadata>& metadata)
{
    if (auto streamDataReceptor = m_streamDataReceptor.toStrongRef())
    {
        const StreamProviderRequirements requirements =
            streamDataReceptor->providerRequirements(Qn::toStreamIndex(getRole()));

        if (requirements.requiredStreamTypes.testFlag(StreamType::metadata))
        {
            NX_VERBOSE(this, "Pushing in-stream metadata to the plugins");
            streamDataReceptor->putData(metadata);
        }
    }
}

void QnLiveStreamProvider::onPrimaryFpsChanged(int primaryFps)
{
    QnMutexLocker lock(&m_liveMutex);
    NX_ASSERT(getRole() == Qn::CR_SecondaryLiveVideo);
    m_primaryFps = primaryFps;
}

QnLiveStreamParams QnLiveStreamProvider::getLiveParams()
{
    QnMutexLocker lock(&m_liveMutex);
    return mergeWithAdvancedParams(m_liveParams);
}

QnLiveStreamParams QnLiveStreamProvider::getActualParams() const
{
    if (isConnectionLost())
        return QnLiveStreamParams();
    QnLiveStreamParams result;
    result.bitrateKbps = bitrateBitsPerSecond() / 1024.0;
    result.fps = getFrameRate();
    const auto streamInfo = m_cameraRes->streamInfo(encoderIndex());
    const auto sizeParts = streamInfo.resolution.split('x');
    if (sizeParts.size() >= 2)
        result.resolution = QSize(sizeParts[0].toInt(), sizeParts[1].toInt());
    result.codec = toString(streamInfo.codec);
    return result;
}

QnAbstractCompressedMetadataPtr QnLiveStreamProvider::getMetadata()
{
    if (!m_metadataReceptor->metadataQueue.isEmpty())
    {
        QnAbstractCompressedMetadataPtr metadata;
        m_metadataReceptor->metadataQueue.pop(metadata);

        if (m_metadataLogger)
        {
            m_metadataLogger->pushData(
                metadata,
                lm("Queue size %1").args(m_metadataReceptor->metadataQueue.size()));
        }

        return metadata;
    }

    if (m_lastMotionMetadata && m_cameraRes->getMotionType() == Qn::MotionType::MT_SoftwareGrid)
    {
        QnMetaDataV1Ptr motionMetadata = std::move(m_lastMotionMetadata);
        m_lastMotionMetadata.reset();
        return motionMetadata;
    }
    else
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
    QnMutexLocker lock(&m_mutex);
    if (!isRunning())
    {
        m_framesSincePrevMediaStreamCheck = CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES+1;
        start();
    }
}

void QnLiveStreamProvider::start(Priority priority)
{
    if (m_canStartThread)
        QnAbstractMediaStreamDataProvider::start(priority);
}

void QnLiveStreamProvider::disableStartThread()
{
    m_canStartThread = false;
}

bool QnLiveStreamProvider::isCameraControlDisabled() const
{
    if (m_doNotConfigureCamera)
        return true;
    const QnVirtualCameraResource* camRes = dynamic_cast<const QnVirtualCameraResource*>(m_resource.data());
    return camRes && camRes->isCameraControlDisabled();
}

void QnLiveStreamProvider::filterMotionByMask(const QnMetaDataV1Ptr& motion)
{
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
}

bool QnLiveStreamProvider::doesFrameSuitMotionAnalysisRequirements(
    const QnCompressedVideoDataPtr& compressedFrame) const
{
    static const int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()
        * SECONDARY_STREAM_MAX_RESOLUTION.height();

    return compressedFrame->width * compressedFrame->height <= maxSquare
        || m_cameraRes->motionStreamIndex().isForced;
}

void QnLiveStreamProvider::checkAndUpdatePrimaryStreamResolution(
    const QnCompressedVideoDataPtr& compressedFrame)
{
    const bool resolutionCheckTimeoutIsExpired = !m_resolutionCheckTimer.isValid()
        || (m_resolutionCheckTimer.elapsed() > PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS);

    const bool updatePrimaryStreamResolution = getRole() == Qn::CR_LiveVideo
        && !m_cameraRes->hasDualStreaming()
        && resolutionCheckTimeoutIsExpired;

    if (updatePrimaryStreamResolution)
    {
        const QSize newResolution = nx::media::getFrameSize(compressedFrame);
        if (newResolution.isValid())
        {
            updateStreamResolution(compressedFrame->channelNumber, newResolution);
            m_resolutionCheckTimer.restart();
        }
    }
}

void QnLiveStreamProvider::updateStreamResolution(int channelNumber, const QSize& newResolution)
{
    if( newResolution == m_videoResolutionByChannelNumber[channelNumber] )
        return;

    NX_VERBOSE(this) << lm("Updating stream resolution");

    m_videoResolutionByChannelNumber[channelNumber] = newResolution;
    onStreamResolutionChanged(channelNumber, newResolution);

    if( getRole() == Qn::CR_SecondaryLiveVideo)
        return;

    //no secondary stream and no motion, may be primary stream is now OK for motion?
    bool newValue = newResolution.width()*newResolution.height() <= MAX_PRIMARY_RES_FOR_SOFT_MOTION
        || m_cameraRes->motionStreamIndex().index == nx::vms::api::StreamIndex::primary;

    bool cameraValue = m_cameraRes->getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability;
    if (newValue != cameraValue)
    {
        if( newValue)
            m_cameraRes->setCameraCapability( Qn::PrimaryStreamSoftMotionCapability, true );
        else
            m_cameraRes->setCameraCapabilities( m_cameraRes->getCameraCapabilities() & ~Qn::PrimaryStreamSoftMotionCapability );
        m_cameraRes->saveProperties();
    }

    QnMutexLocker lock(&m_liveMutex);
    updateSoftwareMotion();
}

void QnLiveStreamProvider::saveMediaStreamParamsIfNeeded(const QnCompressedVideoDataPtr& videoData)
{
    ++m_framesSincePrevMediaStreamCheck;
    if( m_framesSincePrevMediaStreamCheck < CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES ||
        (videoData->flags & QnAbstractMediaData::MediaFlags_AVKey) == 0 )
        return;
    m_framesSincePrevMediaStreamCheck = 0;

    QSize streamResolution = nx::media::getFrameSize(videoData);
    CameraMediaStreamInfo mediaStreamInfo(
        encoderIndex(),
        QSize(streamResolution.width(), streamResolution.height()),
        videoData->compressionType);

    if( m_cameraRes->saveMediaStreamInfoIfNeeded( mediaStreamInfo ) )
        m_cameraRes->savePropertiesAsync();
}

void QnLiveStreamProvider::saveBitrateIfNeeded(
    const QnCompressedVideoDataPtr& /*videoData*/,
    const QnLiveStreamParams& liveParams,
    bool isCameraConfigured)
{
    auto now = qnSyncTime->currentDateTime().toUTC().toString(Qt::ISODate);
    CameraBitrateInfo info(encoderIndex(), std::move(now));

    info.rawSuggestedBitrate = m_cameraRes->rawSuggestBitrateKbps(
        liveParams.quality, liveParams.resolution, liveParams.fps, liveParams.codec) / 1024;
    info.suggestedBitrate = m_cameraRes->suggestBitrateKbps(liveParams, getRole()) / 1024;
    info.actualBitrate = bitrateBitsPerSecond() / 1024.0 / 1024.0 / getNumberOfChannels();

    info.bitratePerGop = m_cameraRes->useBitratePerGop();
    info.bitrateFactor = 1; // TODO: #mux Pass actual value when available [2.6]
    info.numberOfChannels = getNumberOfChannels();

    info.fps = liveParams.fps;
    info.actualFps = getFrameRate();
    info.averageGopSize = getAverageGopSize();
    info.resolution = CameraMediaStreamInfo::resolutionToString(liveParams.resolution);
    info.isConfigured = isCameraConfigured;

    if (m_cameraRes->saveBitrateIfNeeded(info))
    {
        m_cameraRes->savePropertiesAsync();
        NX_VERBOSE(this, lm("bitrateInfo has been updated for %1 stream")
                .arg(QnLexical::serialized(info.encoderIndex)));
    }
}

QnConstResourceAudioLayoutPtr QnLiveStreamProvider::getDPAudioLayout() const
{
    return QnConstResourceAudioLayoutPtr();
}

#endif // ENABLE_DATA_PROVIDERS
