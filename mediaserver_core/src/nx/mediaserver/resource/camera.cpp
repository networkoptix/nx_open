#include "camera.h"

#include <common/static_common_module.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_command.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/serialization/json.h>

#include "camera_advanced_parameters_providers.h"

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
    if (m_defaultAdvancedParametersProviders == nullptr
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
        else if (m_defaultAdvancedParametersProviders)
        {
            NX_DEBUG(this, lm("Get undeclared advanced parameter: %1").arg(id));
            idsByProvider[m_defaultAdvancedParametersProviders].insert(id);
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
    if (m_defaultAdvancedParametersProviders == nullptr
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
        else if (m_defaultAdvancedParametersProviders)
        {
            NX_WARNING(this, lm("Set undeclared advanced parameter: %1").arg(value.id));
            valuesByProvider[m_defaultAdvancedParametersProviders].push_back(value);
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
    const auto getStreamParamiters =
        [&](Qn::StreamIndex streamIndex)
        {
            const auto it = m_streamCapabilityAdvancedProviders.find(streamIndex);
            if (it == m_streamCapabilityAdvancedProviders.end())
                return QnLiveStreamParams();

            return it->second->getParameters();
        };

    QnAdvancedStreamParams paramiters;
    paramiters.primaryStream = getStreamParamiters(Qn::StreamIndex::primary);
    paramiters.secondaryStream = getStreamParamiters(Qn::StreamIndex::secondary);
    return paramiters;
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
    if (requestSquare < kMaxEps || requestSquare > maxResolutionArea) return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff = maxResolutionArea > kMaxEps ? (maxResolutionArea / requestSquare) : INT_MAX;

    for (int i = 0; i < resolutionList.size(); ++i) {
        QSize tmp;

        tmp.setWidth(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].width() + 1), 8));
        tmp.setHeight(qPower2Floor(static_cast<unsigned int>(resolutionList[i].height() - 1), 8));
        float ar1 = getResolutionAspectRatio(tmp);

        tmp.setWidth(qPower2Floor(static_cast<unsigned int>(resolutionList[i].width() - 1), 8));
        tmp.setHeight(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].height() + 1), 8));
        float ar2 = getResolutionAspectRatio(tmp);

        if (aspectRatio != 0 && !qBetween(qMin(ar1,ar2), aspectRatio, qMax(ar1,ar2)))
        {
            continue;
        }

        double square = resolutionList[i].width() * resolutionList[i].height();
        if (square < kMaxEps) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + kMaxEps) {
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
    }

    m_streamCapabilityAdvancedProviders.clear();
    m_defaultAdvancedParametersProviders = nullptr;
    m_advancedParametersProvidersByParameterId.clear();

    const auto driverResult = initializeCameraDriver();
    if (driverResult.errorCode != CameraDiagnostics::ErrorCode::noError)
        return driverResult;

    return initializaAdvancedParamitersProviders();
}

CameraDiagnostics::Result Camera::initializaAdvancedParamitersProviders()
{
    auto allAdvancedParamitersProviders = advancedParametersProviders();
    if (!allAdvancedParamitersProviders.empty())
        m_defaultAdvancedParametersProviders = allAdvancedParamitersProviders.front();

    boost::optional<QSize> baseResolution;
    for (const auto streamType: {Qn::StreamIndex::primary, Qn::StreamIndex::secondary})
    {
        auto streamCapabilities = getStreamCapabilityMap(streamType);
        if (!streamCapabilities.isEmpty())
        {
            auto provider = std::make_unique<StreamCapabilityAdvancedParametersProvider>(
                this, streamCapabilities, streamType,
                baseResolution ? *baseResolution : QSize());

            if (!baseResolution)
                baseResolution = provider->getParameters().resolution;

            // TODO: It might make sence to insert these before driver specific providers.
            allAdvancedParamitersProviders.push_back(provider.get());
            m_streamCapabilityAdvancedProviders.emplace(streamType, std::move(provider));
        }
    }

    QnCameraAdvancedParams advancedParameters;
    for (const auto& provider: allAdvancedParamitersProviders)
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
        m_defaultAdvancedParametersProviders,
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

    StreamCapabilityMap result = getStreamCapabilityMapFromDrives(streamIndex);
    for (auto itr = result.begin(); itr != result.end(); ++itr)
    {
        auto& value = itr.value();
        if (value.isNull())
            value = defaultStreamCapability(itr.key());
    }
    return result;
}

std::vector<Camera::AdvancedParametersProvider*> Camera::advancedParametersProviders()
{
    return {};
}

} // namespace resource
} // namespace mediaserver
} // namespace nx
