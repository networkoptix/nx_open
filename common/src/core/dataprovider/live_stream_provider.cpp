#include "live_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/utils/log/log.h>
#include <nx/utils/safe_direct_connection.h>

#include <motion/motion_detection.h>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_properties.h"
#include "utils/media/h264_utils.h"
#include "utils/media/jpeg_utils.h"
#include "utils/media/nalUnits.h"
#include "utils/common/synctime.h"

#include <utils/media/av_codec_helper.h>
#include <nx/streaming/config.h>
#include <utils/media/hevc_sps.h>

static const int CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES = 1000;
static const int PRIMARY_RESOLUTION_CHECK_TIMEOUT_MS = 10*1000;
static const int SAVE_BITRATE_FRAME = 300; // value TBD
static const float FPS_NOT_INITIALIZED = -1.0;

QnLiveStreamParams::QnLiveStreamParams():
    quality(Qn::QualityNormal),
    secondaryQuality(Qn::SSQualityNotDefined),
    fps(FPS_NOT_INITIALIZED)
{
}

bool QnLiveStreamParams::operator ==(const QnLiveStreamParams& rhs)
{
    return rhs.quality == quality
        && rhs.secondaryQuality == secondaryQuality
        && rhs.fps == fps;
}

bool QnLiveStreamParams::operator !=(const QnLiveStreamParams& rhs) { return !(*this == rhs); }

QnLiveStreamProvider::QnLiveStreamProvider(const QnResourcePtr& res):
    QnAbstractMediaStreamDataProvider(res),
    m_livemutex(QnMutex::Recursive),
    m_qualityUpdatedAtLeastOnce(false),
    m_framesSinceLastMetaData(0),
    m_totalVideoFrames(0),
    m_totalAudioFrames(0),
    m_softMotionRole(Qn::CR_Default),
    m_softMotionLastChannel(0),
    m_videoChannels(1),
    m_framesSincePrevMediaStreamCheck(CHECK_MEDIA_STREAM_ONCE_PER_N_FRAMES+1)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskBinData[i] = (simd128i*) qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight/8, 32);
        memset(m_motionMaskBinData[i], 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight/8);
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        m_motionEstimation[i].setChannelNum(i);
#endif
    }

    m_role = Qn::CR_LiveVideo;
    m_timeSinceLastMetaData.restart();
    m_cameraRes = res.dynamicCast<QnPhysicalCameraResource>();
    NX_ASSERT(m_cameraRes);
    m_prevCameraControlDisabled = m_cameraRes->isCameraControlDisabled();
    m_videoChannels = m_cameraRes->getVideoLayout()->channelCount();
    m_isPhysicalResource = res.dynamicCast<QnPhysicalCameraResource>();
    m_resolutionCheckTimer.invalidate();

    Qn::directConnect(res.data(), &QnResource::videoLayoutChanged, this, [this](const QnResourcePtr&) {
        m_videoChannels = m_cameraRes->getVideoLayout()->channelCount();
        QnMutexLocker mtx( &m_livemutex );
        updateSoftwareMotion();
    });

}


void QnLiveStreamProvider::setOwner(QnSharedResourcePointer<QnAbstractVideoCamera> owner)
{
    m_owner = owner.toWeakRef();
}

QnSharedResourcePointer<QnAbstractVideoCamera> QnLiveStreamProvider::getOwner() const
{
    return m_owner.toStrongRef();
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{
    directDisconnectAll();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        qFreeAligned(m_motionMaskBinData[i]);
}

void QnLiveStreamProvider::setRole(Qn::ConnectionRole role)
{
    QnAbstractMediaStreamDataProvider::setRole(role);

    QnMutexLocker mtx(&m_livemutex);
    updateSoftwareMotion();

    const auto oldParams = m_newLiveParams;
    if (role == Qn::CR_SecondaryLiveVideo)
    {
        m_newLiveParams.quality = m_cameraRes->getSecondaryStreamQuality();
        m_newLiveParams.fps = qMax(1, qMin(
            m_cameraRes->desiredSecondStreamFps(), m_cameraRes->getMaxFps()));
    }

    if (m_newLiveParams != oldParams)
    {
        mtx.unlock();
        pleaseReopenStream();
    }
}

Qn::ConnectionRole QnLiveStreamProvider::getRole() const
{
    QnMutexLocker mtx( &m_livemutex );
    return m_role;
}

int QnLiveStreamProvider::encoderIndex() const
{
    return getRole() == Qn::CR_LiveVideo ? PRIMARY_ENCODER_INDEX
                                         : SECONDARY_ENCODER_INDEX;
}

void QnLiveStreamProvider::setCameraControlDisabled(bool value)
{
    if (!value && m_prevCameraControlDisabled)
        pleaseReopenStream();
    m_prevCameraControlDisabled = value;
}

void QnLiveStreamProvider::setDesiredLiveParams(const QnLiveStreamParams &params)
{
    m_newLiveParams = params;
}

void QnLiveStreamProvider::setSecondaryQuality(Qn::SecondStreamQuality  quality)
{
    {
        QnMutexLocker mtx( &m_livemutex );
        if (m_newLiveParams.secondaryQuality == quality)
            return; // same quality
        m_newLiveParams.secondaryQuality = quality;
        if (m_newLiveParams.secondaryQuality == Qn::SSQualityNotDefined)
            return;
    }

    if (getRole() != Qn::CR_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        if (auto owner = getOwner())
        {
            if (auto lp = owner->getSecondaryReader())
            {
                lp->setQuality(m_cameraRes->getSecondaryStreamQuality());
                lp->onPrimaryFpsUpdated(getLiveParams().fps);
            }
        }

        pleaseReopenStream();
    }
}

void QnLiveStreamProvider::setQuality(Qn::StreamQuality q)
{
    {
        QnMutexLocker mtx( &m_livemutex );
        if (m_newLiveParams.quality == q && m_qualityUpdatedAtLeastOnce)
            return; // same quality

        m_newLiveParams.quality = q;
        m_qualityUpdatedAtLeastOnce = true;

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
    return m_role == Qn::CR_LiveVideo && m_isPhysicalResource;
}

// for live providers only
void QnLiveStreamProvider::setFps(float f)
{
    {
        QnMutexLocker mtx( &m_livemutex );

        if (std::abs(m_newLiveParams.fps - f) < 0.1)
            return; // same fps?

        m_newLiveParams.fps = qMax(1, qMin((int)f, m_cameraRes->getMaxFps()));
        f = m_newLiveParams.fps;
    }

    if (getRole() != Qn::CR_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        if (auto owner = getOwner()) {
            QnLiveStreamProviderPtr lp = owner->getSecondaryReader();
            if (lp)
                lp->onPrimaryFpsUpdated(f);
        }
    }

    pleaseReopenStream();
}

float QnLiveStreamProvider::getDefaultFps() const
{
    float maxFps = m_cameraRes->getMaxFps();
    int reservedSecondStreamFps = m_cameraRes->reservedSecondStreamFps();

    return qMax(1.0, maxFps - reservedSecondStreamFps);
}

bool QnLiveStreamProvider::isMaxFps() const
{
    QnMutexLocker mtx( &m_livemutex );
    return m_newLiveParams.fps >= m_cameraRes->getMaxFps()-0.1;
}

bool QnLiveStreamProvider::needMetaData()
{
    // I assume this function is called once per video frame

    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
    {
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        if (getRole() == roleForMotionEstimation()) {
            for (int i = 0; i < m_videoChannels; ++i)
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

void QnLiveStreamProvider::onStreamReopen()
{
    m_totalVideoFrames = 0;
}

void QnLiveStreamProvider::onGotVideoFrame(const QnCompressedVideoDataPtr& videoData,
                                           const QnLiveStreamParams& currentLiveParams,
                                           bool isCameraControlRequired)
{
    m_totalVideoFrames++;
    m_framesSinceLastMetaData++;

    saveMediaStreamParamsIfNeeded(videoData);
    if (m_totalVideoFrames && (m_totalVideoFrames % SAVE_BITRATE_FRAME) == 0)
        saveBitrateIfNeeded(videoData, currentLiveParams, isCameraControlRequired);

#ifdef ENABLE_SOFTWARE_MOTION_DETECTION

    static const int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();
    bool resoulutionOK = videoData->width * videoData->height <= maxSquare || !m_forcedMotionStream.isEmpty();

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

void QnLiveStreamProvider::onPrimaryFpsUpdated(int newPrimaryStreamFps)
{
    NX_ASSERT(getRole() == Qn::CR_SecondaryLiveVideo);
    // now primary has newFps
    // this is secondary stream
    // need to adjust fps

    Qn::StreamFpsSharingMethod sharingMethod = m_cameraRes->streamFpsSharingMethod();
    int maxPrimaryStreamFps = m_cameraRes->getMaxFps();
    int newSecondaryStreamFps = 0;

    if (secondaryResolutionIsLarge())
    {
        newSecondaryStreamFps = MIN_SECOND_STREAM_FPS;
    }
    else if (sharingMethod == Qn::PixelsFpsSharing)
    {
        //Old comment: minimum between DESIRED_SECOND_STREAM_FPS and what is left;
        newSecondaryStreamFps = qMin(
            m_cameraRes->desiredSecondStreamFps(),
            maxPrimaryStreamFps);

        if (maxPrimaryStreamFps - newPrimaryStreamFps < 2 )
        {
            newSecondaryStreamFps = qMax(
                m_cameraRes->desiredSecondStreamFps() / 2,
                MIN_SECOND_STREAM_FPS);
        }
    }
    else if (sharingMethod == Qn::BasicFpsSharing)
    {
        newSecondaryStreamFps = qMin(
            m_cameraRes->desiredSecondStreamFps(),
            maxPrimaryStreamFps - newPrimaryStreamFps);
    }
    else
    {
        // noSharing
        newSecondaryStreamFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxPrimaryStreamFps);
    }

    setFps(qMax(1.0, newSecondaryStreamFps));
}

QnLiveStreamParams QnLiveStreamProvider::getLiveParams()
{
    QnMutexLocker lock(&m_livemutex);
    if (m_newLiveParams.fps == FPS_NOT_INITIALIZED)
        m_newLiveParams.fps = getDefaultFps();
    return m_newLiveParams;
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

void QnLiveStreamProvider::updateStreamResolution( int channelNumber, const QSize& newResolution )
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
    QnMutexLocker mtx( &m_livemutex );
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
    std::map<QString, QString>* const customStreamParams )
{
    switch( videoData->compressionType )
    {
        case AV_CODEC_ID_H265:
            if (videoData->width > 0 && videoData->height > 0)
            {
                *newResolution = QSize(videoData->width, videoData->height);
            }
            else
            {
                nx::media_utils::hevc::Sps sps;
                if (sps.decodeFromVideoFrame(videoData))
                    *newResolution = QSize(sps.picWidthInLumaSamples, sps.picHeightInLumaSamples);
            }
            break;
        case AV_CODEC_ID_H264:
            nx::media_utils::avc::extractSpsPps(
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
        encoderIndex(),
        QSize(streamResolution.width(), streamResolution.height()),
        videoData->compressionType,
        std::move(customStreamParams) );

    if( m_cameraRes->saveMediaStreamInfoIfNeeded( mediaStreamInfo ) )
        m_cameraRes->saveParamsAsync();
}

void QnLiveStreamProvider::saveBitrateIfNeeded( const QnCompressedVideoDataPtr& videoData,
                                                const QnLiveStreamParams& liveParams,
                                                bool isCameraConfigured )
{
    QSize resoulution;
    std::map<QString, QString> customParams;
    extractMediaStreamParams(videoData, &resoulution, &customParams);

    auto now = qnSyncTime->currentDateTime().toUTC().toString(Qt::ISODate);
    CameraBitrateInfo info(encoderIndex(), std::move(now));

    info.rawSuggestedBitrate = m_cameraRes->rawSuggestBitrateKbps(
        liveParams.quality, resoulution, liveParams.fps) / 1024;
    info.suggestedBitrate = static_cast<float>(m_cameraRes->suggestBitrateKbps(
        liveParams.quality, resoulution, liveParams.fps)) / 1024;
    info.actualBitrate = getBitrateMbps() / getNumberOfChannels();

    info.bitratePerGop = m_cameraRes->bitratePerGopType();
    info.bitrateFactor = 1; // TODO: #mux Pass actual value when avaliable [2.6]
    info.numberOfChannels = getNumberOfChannels();

    info.fps = liveParams.fps;
    info.actualFps = getFrameRate();
    info.averageGopSize = getAverageGopSize();
    info.resolution = CameraMediaStreamInfo::resolutionToString(resoulution);
    info.isConfigured = isCameraConfigured;

    if (m_cameraRes->saveBitrateIfNeeded(info))
    {
        m_cameraRes->saveParamsAsync();
        NX_LOG(lm("QnLiveStreamProvider: bitrateInfo has been updated for %1 stream")
                .arg(info.encoderIndex), cl_logINFO);
    }
}

#endif // ENABLE_DATA_PROVIDERS
