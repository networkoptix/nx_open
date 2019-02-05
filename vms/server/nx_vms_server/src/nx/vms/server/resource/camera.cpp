#include "camera.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource_management/resource_data_pool.h>
#include <providers/live_stream_provider.h>
#include <utils/media/av_codec_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/serialization/json.h>

#include "camera_advanced_parameters_providers.h"
#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <media_server/media_server_module.h>
#include <nx/streaming/archive_stream_reader.h>
#include <plugins/utils/multisensor_data_provider.h>

static const std::set<QString> kSupportedCodecs = {"MJPEG", "H264", "H265"};

namespace nx {
namespace vms::server {
namespace resource {

const float Camera::kMaxEps = 0.01f;

Camera::Camera(QnMediaServerModule* serverModule):
    QnVirtualCameraResource(serverModule ? serverModule->commonModule() : nullptr),
    nx::vms::server::ServerModuleAware(serverModule),
    m_channelNumber(0)
{
    setFlags(Qn::local_live_cam);
    m_lastInitTime.invalidate();

    connect(this, &Camera::groupIdChanged,
        [this]()
        {
            NX_CRITICAL(!isInitializationInProgress(), "Initialization should fail");

            const auto status = getStatus();
            if (!NX_ASSERT(status != Qn::Online && status != Qn::Recording, status))
                setStatus(Qn::Offline);
        });

    connect(this, &QnResource::initializedChanged,
        [this]()
        {
            // m_initMutex is locked down the stack.
            fixInputPortMonitoring();
        });

    const auto updateIoCache =
        [this](const QnResourcePtr&, const QString& id, bool value, qint64 timestamp)
        {
            NX_DEBUG(this, "Port %1 is changed to %2 (%3)", id, value, timestamp);
            QnMutexLocker lk(&m_ioPortStatesMutex);
            m_ioPortStatesCache[id] = QnIOStateData(id, value, timestamp);
        };

    connect(this, &Camera::inputPortStateChanged, updateIoCache);
    connect(this, &Camera::outputPortStateChanged, updateIoCache);
    m_timeOffset = std::make_shared<nx::streaming::rtp::TimeOffset>();
}

Camera::~Camera()
{
    // Needed because of the forward declaration.
}

void Camera::blockingInit()
{
    if (!init())
    {
        //init is running in another thread, waiting for it to complete...
        QnMutexLocker lk(&m_initMutex);
    }
}

int Camera::getChannel() const
{
    QnMutexLocker lock( &m_mutex );
    return m_channelNumber;
}

QnAbstractPtzController* Camera::createPtzController() const
{
    QnAbstractPtzController* result = createPtzControllerInternal();
    if (!result)
        return result;

    /* Do some sanity checking. */
    Ptz::Capabilities capabilities = result->getCapabilities();
    if((capabilities & Ptz::LogicalPositioningPtzCapability)
        && !(capabilities & Ptz::AbsolutePtzCapabilities))
    {
        auto message =
            lit("Logical position space capability is defined for a PTZ controller that does not support absolute movement. %1 %2")
            .arg(getName())
            .arg(getUrl());

        qDebug() << message;
        NX_WARNING(this, message);
    }

    if((capabilities & Ptz::DevicePositioningPtzCapability)
        && !(capabilities & Ptz::AbsolutePtzCapabilities))
    {
        auto message =
            lit("Device position space capability is defined for a PTZ controller that does not support absolute movement. %1 %2")
            .arg(getName())
            .arg(getUrl());

        qDebug() << message;
        NX_ERROR(this, message.toLatin1());
    }

    return result;
}

QString Camera::defaultCodec() const
{
    return QnAvCodecHelper::codecIdToString(AV_CODEC_ID_H264);
}

void Camera::setUrl(const QString &urlStr)
{
    QnVirtualCameraResource::setUrl( urlStr ); /* This call emits, so we should not invoke it under lock. */

    QnMutexLocker lock( &m_mutex );
    QUrl url( urlStr );
    m_channelNumber = QUrlQuery( url.query() ).queryItemValue( QLatin1String( "channel" ) ).toInt();
    setHttpPort( url.port( httpPort() ) );
    if (m_channelNumber > 0)
        m_channelNumber--; // convert human readable channel in range [1..x] to range [0..x-1]
}

QnCameraAdvancedParamValueMap Camera::getAdvancedParameters(const QSet<QString>& ids)
{
    QnMutexLocker lock(&m_initMutex);
    if (!isInitialized())
        return {};

    if (m_defaultAdvancedParametersProvider == nullptr
        && m_advancedParametersProvidersByParameterId.empty())
    {
        NX_ASSERT(false, "Get advanced parameters from camera with no providers");
        return {};
    }

    std::map<AdvancedParametersProvider*, QSet<QString>> idsByProvider;
    for (const auto& id: ids)
    {
        const auto it = m_advancedParametersProvidersByParameterId.find(id);
        if (it != m_advancedParametersProvidersByParameterId.end())
        {
            idsByProvider[it->second].insert(id);
        }
        else if (m_defaultAdvancedParametersProvider)
        {
            NX_DEBUG(this, lm("Get undeclared advanced parameter: %1").arg(id));
            idsByProvider[m_defaultAdvancedParametersProvider].insert(id);
        }
        else
        {
            NX_WARNING(this, lm("No provider to set parameter: %1").arg(id));
        }
    }

    QnCameraAdvancedParamValueMap result;
    for (auto& providerIds: idsByProvider)
    {
        const auto provider = providerIds.first;
        const auto& ids = providerIds.second;

        auto values = provider->get(ids);
        NX_VERBOSE(this, lm("Get advanced parameters %1 by %2, result %3").args(
            containerString(ids), provider, containerString(values)));

        for (auto it = values.begin(); it != values.end(); ++it)
            result.insert(it.key(), it.value());
    }

    return result;
}

boost::optional<QString> Camera::getAdvancedParameter(const QString& id)
{
    const auto values = getAdvancedParameters({id});
    if (values.contains(id))
        return values.value(id);

    return boost::none;
}

QSet<QString> Camera::setAdvancedParameters(const QnCameraAdvancedParamValueMap& values)
{
    QnMutexLocker lock(&m_initMutex);
    if (m_defaultAdvancedParametersProvider == nullptr
        && m_advancedParametersProvidersByParameterId.empty())
    {
        // NOTE: It may sometimes occur if we are trying to set some parameters on the never
        // initialised camera.
        NX_VERBOSE(this, "Set advanced parameters from camera with no providers: %1", values);
        return {};
    }

    std::map<AdvancedParametersProvider*, QnCameraAdvancedParamValueList> valuesByProvider;
    for (const auto& value: values.toValueList())
    {
        const auto it = m_advancedParametersProvidersByParameterId.find(value.id);
        if (it != m_advancedParametersProvidersByParameterId.end())
        {
            valuesByProvider[it->second].push_back(value);
        }
        else if (m_defaultAdvancedParametersProvider)
        {
            NX_WARNING(this, "Set undeclared advanced parameter: %1", value.id);
            valuesByProvider[m_defaultAdvancedParametersProvider].push_back(value);
        }
        else
        {
            NX_WARNING(this, "No provider to set parameter: %1", value.id);
        }
    }

    QSet<QString> result;
    for (auto& providerValues: valuesByProvider)
    {
        const auto provider = providerValues.first;
        const auto& values = providerValues.second;

        auto ids = provider->set(values);
        NX_VERBOSE(this, "Set advanced parameters %1 by %2, result %3", containerString(values),
            provider, containerString(ids));

        result += std::move(ids);
    }

    return result;
}

bool Camera::setAdvancedParameter(const QString& id, const QString& value)
{
    QnCameraAdvancedParamValueMap parameters;
    parameters.insert(id, value);
    return setAdvancedParameters(parameters).contains(id);
}

QnAdvancedStreamParams Camera::advancedLiveStreamParams() const
{
    const auto getStreamParameters =
        [&](nx::vms::api::MotionStreamType streamIndex)
        {
            QnMutexLocker lock(&m_initMutex);
            if (!isInitialized())
                return QnLiveStreamParams();

            const auto it = m_streamCapabilityAdvancedProviders.find(streamIndex);
            if (it == m_streamCapabilityAdvancedProviders.end())
                return QnLiveStreamParams();

            return it->second->getParameters();
        };

    QnAdvancedStreamParams parameters;
    parameters.primaryStream = getStreamParameters(nx::vms::api::MotionStreamType::primary);
    parameters.secondaryStream = getStreamParameters(nx::vms::api::MotionStreamType::secondary);
    return parameters;
}

float Camera::getResolutionAspectRatio(const QSize& resolution)
{
    if (resolution.height() == 0)
        return 0;
    float result = static_cast<double>(resolution.width()) / resolution.height();
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    if (resolution.width() == 720 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(4.0 / 3.0);
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    else if (resolution.width() == 960 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(16.0 / 9.0);
    return result;
}

/*static*/ QSize Camera::getNearestResolution(
    const QSize& resolution,
    float desiredAspectRatio,
    double maxResolutionArea,
    const QList<QSize>& resolutionList,
    double* outCoefficient)
{
    if (outCoefficient)
        *outCoefficient = INT_MAX;

    double requestSquare = resolution.width() * resolution.height();
    if (requestSquare < kMaxEps || requestSquare > maxResolutionArea)
        return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff =
        (maxResolutionArea > kMaxEps) ? (maxResolutionArea / requestSquare) : INT_MAX;
    /*
        Typical aspect ratios:
         w   h   ratio   A     B
        21 / 9 = 2.(3)        1.31
        16 / 9 = 1.(7)  1.31  1.14
        14 / 9 = 1.(5)  1.14  1.67
        12 / 9 = 1.(3)  1.67  1.33
         9 / 9 = 1.(0)  1.33
        ------
         A = higher ratio / current ratio
         B = current ratio / lower ratio

        We consider that one resolution is similar to another if their aspect ratios differ
        no more than (1 + kEpsilon) times. kEpsilon estimation is heuristically inferred from
        the table above.
    */
    static const float kEpsilon = 0.10f;
    for (int i = 0; i < resolutionList.size(); ++i)
    {

        const double nextAspectRatio = getResolutionAspectRatio(resolutionList[i]);
        const bool currentRatioFitsDesirable = nextAspectRatio * (1 - kEpsilon) < desiredAspectRatio
            && desiredAspectRatio < nextAspectRatio * (1 + kEpsilon);

        if (desiredAspectRatio != 0 && !currentRatioFitsDesirable)
            continue;

        const double square = resolutionList[i].width() * resolutionList[i].height();
        if (square < kMaxEps)
            continue;

        const double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + kMaxEps)
        {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
            if (outCoefficient)
                *outCoefficient = bestMatchCoeff;
        }
    }

    return bestIndex >= 0 ? resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

/*static*/ QSize Camera::closestResolution(
    const QSize& idealResolution,
    float desiredAspectRatio,
    const QSize& maxResolution,
    const QList<QSize>& resolutionList,
    double* outCoefficient)
{
    const auto maxResolutionArea = double(maxResolution.width()) * double(maxResolution.height());
    QSize result = getNearestResolution(
        idealResolution,
        desiredAspectRatio,
        maxResolutionArea,
        resolutionList,
        outCoefficient);

    if (result == EMPTY_RESOLUTION_PAIR)
    {
        // Try to get resolution ignoring the aspect ratio.
        result = getNearestResolution(
            idealResolution,
            0.0f,
            maxResolutionArea,
            resolutionList,
            outCoefficient);
    }

    return result;
}

/*static*/ QSize Camera::closestSecondaryResolution(
    float desiredAspectRatio,
    const QList<QSize>& resolutionList)
{
    QSize selectedResolution = Camera::closestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION, desiredAspectRatio,
        SECONDARY_STREAM_MAX_RESOLUTION, resolutionList);

    if (selectedResolution == EMPTY_RESOLUTION_PAIR)
    {
        selectedResolution = Camera::closestResolution(
            SECONDARY_STREAM_DEFAULT_RESOLUTION, desiredAspectRatio,
            UNLIMITED_RESOLUTION, resolutionList);
    }
    return selectedResolution;
}

CameraDiagnostics::Result Camera::initInternal()
{
    auto resData = resourceData();
    auto timeoutSec = resData.value<int>(ResourceDataKey::kUnauthorizedTimeoutSec);
    auto credentials = getAuth();
    auto status = getStatus();
    if (timeoutSec > 0 &&
        m_lastInitTime.isValid() &&
        m_lastInitTime.elapsed() < timeoutSec * 1000 &&
        status == Qn::Unauthorized &&
        m_lastCredentials == credentials)
    {
        return CameraDiagnostics::NotAuthorisedResult(getUrl());
    }

    m_lastInitTime.restart();
    m_lastCredentials = credentials;

    m_mediaTraits = resData.value<nx::media::CameraTraits>(
        ResourceDataKey::kMediaTraits,
        nx::media::CameraTraits());

    setCameraCapability(Qn::CameraTimeCapability, true);

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    const auto driverResult = initializeCameraDriver();
    if (driverResult.errorCode != CameraDiagnostics::ErrorCode::noError)
        return driverResult;

    return initializeAdvancedParametersProviders();
}

void Camera::initializationDone()
{
    base_type::initializationDone();

    // TODO: Find out is it's ever required, monitoring resource state change should be enough!
    QnMutexLocker lk(&m_initMutex);
    fixInputPortMonitoring();
}

StreamCapabilityMap Camera::getStreamCapabilityMapFromDriver(nx::vms::api::MotionStreamType streamIndex)
{
    // Implementation may be overloaded in a driver.
	return StreamCapabilityMap();
}

nx::media::CameraTraits Camera::mediaTraits() const
{
    return m_mediaTraits;
}

QnAbstractPtzController* Camera::createPtzControllerInternal() const
{
    return nullptr;
}

void Camera::startInputPortStatesMonitoring()
{
}

void Camera::stopInputPortStatesMonitoring()
{
}

CameraDiagnostics::Result Camera::initializeAdvancedParametersProviders()
{
    m_streamCapabilityAdvancedProviders.clear();
    m_defaultAdvancedParametersProvider = nullptr;
    m_advancedParametersProvidersByParameterId.clear();

    std::vector<Camera::AdvancedParametersProvider*> allProviders;
    boost::optional<QSize> baseResolution;
    const StreamCapabilityMaps streamCapabilityMaps = {
        {MotionStreamType::primary, getStreamCapabilityMap(MotionStreamType::primary)},
        {MotionStreamType::secondary, getStreamCapabilityMap(MotionStreamType::secondary)}
    };

    const auto traits = mediaTraits();
    for (const auto streamType: {MotionStreamType::primary, MotionStreamType::secondary})
    {
        //auto streamCapabilities = getStreamCapabilityMap(streamType);
        if (!streamCapabilityMaps[streamType].isEmpty())
        {
            auto provider = std::make_unique<StreamCapabilityAdvancedParametersProvider>(
                this,
                streamCapabilityMaps,
                traits,
                streamType,
                baseResolution ? *baseResolution : QSize());

            if (!baseResolution)
                baseResolution = provider->getParameters().resolution;

            // TODO: It might make sence to insert these before driver specific providers.
            allProviders.push_back(provider.get());
            m_streamCapabilityAdvancedProviders.emplace(streamType, std::move(provider));
        }
    }

    auto driverProviders = advancedParametersProviders();
    if (!driverProviders.empty())
        m_defaultAdvancedParametersProvider = driverProviders.front();

    allProviders.insert(allProviders.end(), driverProviders.begin(), driverProviders.end());
    QnCameraAdvancedParams advancedParameters;
    for (const auto& provider: allProviders)
    {
        auto providerParameters = provider->descriptions();
        for (const auto& id: providerParameters.allParameterIds())
            m_advancedParametersProvidersByParameterId.emplace(id, provider);

        if (advancedParameters.groups.empty())
            advancedParameters = std::move(providerParameters);
        else
            advancedParameters.merge(providerParameters);
    }

    NX_VERBOSE(this, "Default advanced parameters provider %1, providers by params: %2",
        m_defaultAdvancedParametersProvider,
        containerString(m_advancedParametersProvidersByParameterId));

    advancedParameters.packet_mode = resourceData()
        .value<bool>(ResourceDataKey::kNeedToReloadAllAdvancedParametersAfterApply, false);

    QnCameraAdvancedParamsReader::setParamsToResource(this->toSharedPointer(), advancedParameters);
    return CameraDiagnostics::NoErrorResult();
}

StreamCapabilityMap Camera::getStreamCapabilityMap(nx::vms::api::MotionStreamType streamIndex)
{
    auto defaultStreamCapability = [this](const StreamCapabilityKey& key)
    {
        nx::media::CameraStreamCapability result;
        result.minBitrateKbps = rawSuggestBitrateKbps(Qn::StreamQuality::lowest, key.resolution, 1);
        result.maxBitrateKbps = rawSuggestBitrateKbps(Qn::StreamQuality::highest, key.resolution, getMaxFps());
        result.maxFps = getMaxFps();
        return result;
    };

    using namespace nx::media;
    auto mergeField = [](int& dst, const int& src)
    {
        if (dst == 0)
            dst = src;
    };

    StreamCapabilityMap result = getStreamCapabilityMapFromDriver(streamIndex);
    for (auto itr = result.begin(); itr != result.end();)
    {
        if (kSupportedCodecs.count(itr.key().codec))
        {
            ++itr;
            continue;
        }

        NX_DEBUG(this, lm("Remove unsupported stream capability %1").args(itr.key()));
        itr = result.erase(itr);
    }

    for (auto itr = result.begin(); itr != result.end(); ++itr)
    {
        auto& value = itr.value();
        const auto defaultValue = defaultStreamCapability(itr.key());
        mergeField(value.minBitrateKbps, defaultValue.minBitrateKbps);
        mergeField(value.maxBitrateKbps, defaultValue.maxBitrateKbps);
        mergeField(value.defaultBitrateKbps, defaultValue.defaultBitrateKbps);
        mergeField(value.defaultFps, defaultValue.defaultFps);
        mergeField(value.maxFps, defaultValue.maxFps);
    }

    return result;
}

std::vector<Camera::AdvancedParametersProvider*> Camera::advancedParametersProviders()
{
    return {};
}

QnConstResourceAudioLayoutPtr Camera::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (isAudioEnabled())
    {
        if (auto liveProvider = dynamic_cast<const QnLiveStreamProvider*>(dataProvider))
        {
            auto result = liveProvider->getDPAudioLayout();
            if (result)
                return result;
        }
    }
    return base_type::getAudioLayout(dataProvider);
}

QnAudioTransmitterPtr Camera::getAudioTransmitter()
{
    if (!isInitialized())
        return nullptr;
    return m_audioTransmitter;
}

void Camera::setLastMediaIssue(const CameraDiagnostics::Result& issue)
{
    QnMutexLocker lk(&m_mutex);
    m_lastMediaIssue = issue;
}

CameraDiagnostics::Result Camera::getLastMediaIssue() const
{
    QnMutexLocker lk(&m_mutex);
    return m_lastMediaIssue;
}

QnAbstractStreamDataProvider* Camera::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole role)
{
    const auto camera = resource.dynamicCast<Camera>();
    NX_ASSERT(camera);
    if (!camera)
        return nullptr;

    if (role == Qn::CR_SecondaryLiveVideo && !camera->hasDualStreaming())
        return nullptr;

    switch (role)
    {
        case Qn::CR_SecondaryLiveVideo:
        case Qn::CR_Default:
        case Qn::CR_LiveVideo:
        {
            QnAbstractStreamDataProvider* result = nullptr;

            #if defined(ENABLE_ONVIF)
                auto shouldAppearAsSingleChannel = camera->resourceData().value<bool>(
                    ResourceDataKey::kShouldAppearAsSingleChannel);

                result = shouldAppearAsSingleChannel
                    ? new nx::plugins::utils::MultisensorDataProvider(camera)
                    : camera->createLiveDataProvider();
            #else
                result = camera->createLiveDataProvider();
            #endif
            if (result)
                result->setRole(role);
            return result;
        }
        case Qn::CR_Archive:
        {
            if (QnAbstractStreamDataProvider* result = camera->createArchiveDataProvider())
                return result;

            QnAbstractArchiveDelegate* archiveDelegate = camera->createArchiveDelegate();
            if (!archiveDelegate)
                archiveDelegate = new QnServerArchiveDelegate(camera->serverModule()); //< Default value.
            if (!archiveDelegate)
                return nullptr;

            auto archiveReader = new QnArchiveStreamReader(camera);
            archiveReader->setCycleMode(false);
            archiveReader->setArchiveDelegate(archiveDelegate);
            return archiveReader;
        }
        default:
            NX_ASSERT(false, "There are no other roles");
            break;
    }
    return nullptr;
}

int Camera::getMaxChannels() const
{
    bool shouldAppearAsSingleChannel = resourceData().value<bool>(
        ResourceDataKey::kShouldAppearAsSingleChannel);
    if (shouldAppearAsSingleChannel)
        return 1;
    return getMaxChannelsFromDriver();
}
void Camera::inputPortListenerAttached()
{
    QnMutexLocker lk(&m_initMutex);
    ++m_inputPortListenerCount;
    fixInputPortMonitoring();
}

void Camera::inputPortListenerDetached()
{
    QnMutexLocker lk(&m_initMutex);
    if (m_inputPortListenerCount == 0)
    {
        NX_ASSERT(false, "Detached input port listener without attach");
    }
    else
    {
        --m_inputPortListenerCount;
        fixInputPortMonitoring();
    }
}

QnIOStateDataList Camera::ioPortStates() const
{
    QnMutexLocker lock(&m_mutex);
    QnIOStateDataList states;
    for (const auto& [id, state]: m_ioPortStatesCache)
        states.push_back(state);
    return states;
}

bool Camera::setOutputPortState(
    const QString& /*portId*/,
    bool /*value*/,
    unsigned int /*autoResetTimeoutMs*/)
{
    return false;
}

void Camera::fixInputPortMonitoring()
{
    if (isInitialized() && m_inputPortListenerCount)
    {
        if (!m_inputPortListeningInProgress && hasCameraCapabilities(Qn::InputPortCapability))
        {
            NX_DEBUG(this, "Start input port monitoring");
            startInputPortStatesMonitoring();
            m_inputPortListeningInProgress = true;
        }
    }
    else
    {
        if (m_inputPortListeningInProgress)
        {
            NX_DEBUG(this, "Stop input port monitoring");
            stopInputPortStatesMonitoring();
            m_inputPortListeningInProgress = false;
        }
    }
}

QnTimePeriodList Camera::getDtsTimePeriods(
    qint64 /*startTimeMs*/,
    qint64 /*endTimeMs*/,
    int /*detailLevel*/,
    bool /*keepSmalChunks*/,
    int /*limit*/,
    Qt::SortOrder /*sortOrder*/)
{
    return QnTimePeriodList();
}

} // namespace resource
} // namespace vms::server
} // namespace nx
