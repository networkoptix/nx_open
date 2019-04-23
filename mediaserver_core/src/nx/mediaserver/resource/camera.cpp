#include "camera.h"

#include <common/static_common_module.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_command.h>
#include <utils/media/av_codec_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/model_functions.h>

#include "camera_advanced_parameters_providers.h"

static const std::set<QString> kSupportedCodecs = {"MJPEG", "H264", "H265"};

namespace nx {
namespace mediaserver {
namespace resource {

namespace {

class GetAdvancedParametersCommand: public QnResourceCommand
{
public:
    GetAdvancedParametersCommand(
        const QnResourcePtr& resource,
        const QSet<QString>& ids,
        std::function<void(const QnCameraAdvancedParamValueMap&)> handler)
    :
        QnResourceCommand(resource),
        m_ids(ids),
        m_handler(std::move(handler))
    {
    }

    bool execute() override
    {
        const auto camera = m_resource.dynamicCast<Camera>();
        NX_CRITICAL(camera);

        QnCameraAdvancedParamValueMap values;
        if (isConnectedToTheResource())
            values = camera->getAdvancedParameters(m_ids);

        m_handler(values);
        return true;
    }

    void beforeDisconnectFromResource() override
    {
    }

private:
    QSet<QString> m_ids;
    std::function<void(const QnCameraAdvancedParamValueMap&)> m_handler;
};

class SetAdvancedParametersCommand: public QnResourceCommand
{
public:
    SetAdvancedParametersCommand(
        const QnResourcePtr& resource,
        const QnCameraAdvancedParamValueMap& values,
        std::function<void(const QSet<QString>&)> handler)
    :
        QnResourceCommand(resource),
        m_values(values),
        m_handler(std::move(handler))
    {
    }

    bool execute() override
    {
        const auto camera = m_resource.dynamicCast<Camera>();
        NX_CRITICAL(camera);

        QSet<QString> ids;
        if (isConnectedToTheResource())
            ids = camera->setAdvancedParameters(m_values);

        m_handler(ids);
        return true;
    }

    void beforeDisconnectFromResource() override
    {
    }

private:
    QnCameraAdvancedParamValueMap m_values;
    std::function<void(const QSet<QString>&)> m_handler;
};

} // namespace

const float Camera::kMaxEps = 0.01f;

Camera::Camera(QnCommonModule* commonModule):
    QnVirtualCameraResource(commonModule),
    m_channelNumber(0)
{
    setFlags(Qn::local_live_cam);
    m_lastInitTime.invalidate();
}

Camera::~Camera()
{
    // Needed because of the forward declaration.
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
        NX_LOG(message, cl_logWARNING);
    }

    if((capabilities & Ptz::DevicePositioningPtzCapability)
        && !(capabilities & Ptz::AbsolutePtzCapabilities))
    {
        auto message =
            lit("Device position space capability is defined for a PTZ controller that does not support absolute movement. %1 %2")
            .arg(getName())
            .arg(getUrl());

        qDebug() << message;
        NX_LOG(message.toLatin1(), cl_logERROR);
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
        NX_ASSERT(this, "Get advanced parameters from camera with no providers");
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
    if (!isInitialized())
        return {};

    if (m_defaultAdvancedParametersProvider == nullptr
        && m_advancedParametersProvidersByParameterId.empty())
    {
        NX_ASSERT(this, "Set advanced parameters from camera with no providers");
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
            NX_WARNING(this, lm("Set undeclared advanced parameter: %1").arg(value.id));
            valuesByProvider[m_defaultAdvancedParametersProvider].push_back(value);
        }
        else
        {
            NX_WARNING(this, lm("No provider to set parameter: %1").arg(value.id));
        }
    }

    QSet<QString> result;
    for (auto& providerValues: valuesByProvider)
    {
        const auto provider = providerValues.first;
        const auto& values = providerValues.second;

        auto ids = provider->set(values);
        NX_VERBOSE(this, lm("Set advanced parameters %1 by %2, result %3").args(
            containerString(values), provider, containerString(ids)));

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

void Camera::getAdvancedParametersAsync(
    const QSet<QString>& ids,
    std::function<void(const QnCameraAdvancedParamValueMap&)> handler)
{
    addCommandToProc(std::make_shared<GetAdvancedParametersCommand>(
        toSharedPointer(this), ids, std::move(handler)));
}

void Camera::setAdvancedParametersAsync(
    const QnCameraAdvancedParamValueMap& values,
    std::function<void(const QSet<QString>&)> handler)
{
    addCommandToProc(std::make_shared<SetAdvancedParametersCommand>(
        toSharedPointer(this), values, std::move(handler)));
}

QnAdvancedStreamParams Camera::advancedLiveStreamParams() const
{
    const auto getStreamParameters =
        [&](Qn::StreamIndex streamIndex)
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
    parameters.primaryStream = getStreamParameters(Qn::StreamIndex::primary);
    parameters.secondaryStream = getStreamParameters(Qn::StreamIndex::secondary);
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

QSize Camera::getNearestResolution(
    const QSize& resolution,
    float aspectRatio,
    double maxResolutionArea,
    const QList<QSize>& resolutionList,
    double* coeff)
{
    if (coeff)
        *coeff = INT_MAX;

    double requestSquare = resolution.width() * resolution.height();
    if (requestSquare < kMaxEps || requestSquare > maxResolutionArea)
        return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff =
        maxResolutionArea > kMaxEps ? (maxResolutionArea / requestSquare) : INT_MAX;

    for (int i = 0; i < resolutionList.size(); ++i)
    {
        QSize tmp;

        tmp.setWidth(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].width() + 1), 8));
        tmp.setHeight(qPower2Floor(static_cast<unsigned int>(resolutionList[i].height() - 1), 8));
        const float ar1 = getResolutionAspectRatio(tmp);

        tmp.setWidth(qPower2Floor(static_cast<unsigned int>(resolutionList[i].width() - 1), 8));
        tmp.setHeight(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].height() + 1), 8));
        const float ar2 = getResolutionAspectRatio(tmp);

        if (aspectRatio != 0 && !qBetween(qMin(ar1,ar2), aspectRatio, qMax(ar1,ar2)))
            continue;

        const double square = resolutionList[i].width() * resolutionList[i].height();
        if (square < kMaxEps)
            continue;

        const double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + kMaxEps)
        {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
            if (coeff)
                *coeff = bestMatchCoeff;
        }
    }

    return bestIndex >= 0 ? resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

QSize Camera::closestResolution(
    const QSize& idealResolution,
    float aspectRatio,
    const QSize& maxResolution,
    const QList<QSize>& resolutionList,
    double* outCoefficient)
{
    const auto maxResolutionArea = double(maxResolution.width()) * double(maxResolution.height());
    QSize result = getNearestResolution(
        idealResolution,
        aspectRatio,
        maxResolutionArea,
        resolutionList,
        outCoefficient);

    if (result == EMPTY_RESOLUTION_PAIR)
    {
        result = getNearestResolution(
            idealResolution,
            0.0,
            maxResolutionArea,
            resolutionList,
            outCoefficient); //< Try to get resolution ignoring aspect ration
    }

    return result;
}

CameraDiagnostics::Result Camera::initInternal()
{
    if (qnStaticCommon)
    {
        auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
        int timeoutSec = resData.value<int>(Qn::kUnauthrizedTimeoutParamName);
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
            Qn::kMediaTraits,
            nx::media::CameraTraits());
    }

    m_streamCapabilityAdvancedProviders.clear();
    m_defaultAdvancedParametersProvider = nullptr;
    m_advancedParametersProvidersByParameterId.clear();

    const auto driverResult = initializeCameraDriver();
    if (driverResult.errorCode != CameraDiagnostics::ErrorCode::noError)
        return driverResult;

    return initializeAdvancedParametersProviders();
}

nx::media::CameraTraits Camera::mediaTraits() const
{
    return m_mediaTraits;
}

QnAbstractPtzController* Camera::createPtzControllerInternal() const
{
    return nullptr;
}

CameraDiagnostics::Result Camera::initializeAdvancedParametersProviders()
{
    std::vector<Camera::AdvancedParametersProvider*> allProviders;
    boost::optional<QSize> baseResolution;
    const StreamCapabilityMaps streamCapabilityMaps = {
        {Qn::StreamIndex::primary, getStreamCapabilityMap(Qn::StreamIndex::primary)},
        {Qn::StreamIndex::secondary, getStreamCapabilityMap(Qn::StreamIndex::secondary)}
    };

    const auto traits = mediaTraits();
    for (const auto streamType: {Qn::StreamIndex::primary, Qn::StreamIndex::secondary})
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

    NX_VERBOSE(this, lm("Default advanced parameters provider %1, providers by params: %2").args(
        m_defaultAdvancedParametersProvider,
        containerString(m_advancedParametersProvidersByParameterId)));

    QnCameraAdvancedParamsReader::setParamsToResource(this->toSharedPointer(), advancedParameters);
    return CameraDiagnostics::NoErrorResult();
}

StreamCapabilityMap Camera::getStreamCapabilityMap(Qn::StreamIndex streamIndex)
{
    auto defaultStreamCapability = [this](const StreamCapabilityKey& key)
    {
        nx::media::CameraStreamCapability result;
        result.minBitrateKbps = rawSuggestBitrateKbps(Qn::QualityLowest, key.resolution, 1);
        result.maxBitrateKbps = rawSuggestBitrateKbps(Qn::QualityHighest, key.resolution, getMaxFps());
        result.maxFps = getMaxFps();
        return result;
    };

    using namespace nx::media;
    auto mergeField = [](int& dst, const int& src)
    {
        if (dst == 0)
            dst = src;
    };

    StreamCapabilityMap result = getStreamCapabilityMapFromDrives(streamIndex);
    for (auto itr = result.begin(); itr != result.end();)
    {
        if (kSupportedCodecs.count(itr.key().codec))
        {
            ++itr;
            continue;
        }

        NX_DEBUG(this, lm("Remove unsuported stream capability %1").args(itr.key()));
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

void Camera::reopenStream(Qn::StreamIndex streamIndex)
{
    auto camera = qnCameraPool ->getVideoCamera(toSharedPointer(this));
    if (!camera)
        return;

    QnLiveStreamProviderPtr reader;
    if (streamIndex == Qn::StreamIndex::primary)
        reader = camera->getPrimaryReader();
    else if (streamIndex == Qn::StreamIndex::secondary)
        reader = camera->getSecondaryReader();

    if (reader && reader->isRunning())
    {
        NX_DEBUG(this, lm("Camera [%1], reopen reader: %2").args(
            getId(), QnLexical::serialized(streamIndex)));
        reader->pleaseReopenStream();
    }
}

} // namespace resource
} // namespace mediaserver
} // namespace nx
