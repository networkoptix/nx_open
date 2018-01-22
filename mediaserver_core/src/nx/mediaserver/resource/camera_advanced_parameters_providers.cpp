#include "camera_advanced_parameters_providers.h"

#include <camera/camera_pool.h>
#include <camera/video_camera.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver {
namespace resource {

namespace {

static const auto kCodec = lit("codec");
static const auto kResolution = lit("resolution");
static const auto kBitrateKpbs = lit("bitrateKbps");
static const auto kFps = lit("fps");
static const auto kResetToDefaults = lit("resetToDefaults");

// TODO: Makes sense to somewhere in nx::utils.
static const QString sizeToString(const QSize& size)
{
    return lm("%1x%2").args(size.width(), size.height());
}

// TODO: Makes sense to somewhere in nx::utils.
static boost::optional<QSize> sizeFromString(const QString& size)
{
    const auto sizeParts = size.splitRef(QChar('x'));
    if (sizeParts.size() != 2)
        return boost::none;

    const auto width = sizeParts.at(0).toInt();
    const auto heigth = sizeParts.at(1).toInt();
    if (width <= 0 || heigth <= 0)
        return boost::none;

    return QSize{width, heigth};
}

// TODO: Makes sense to move into QnCameraAdvancedParam
static QString makeRange(QStringList list)
{
    QCollator collator;
    collator.setNumericMode(true);

    std::sort(list.begin(), list.end(),
        [&](const QString& left, const QString& right) { return collator.compare(left, right) < 0; });

    return list.join(',');
}

// TODO: Makes sense to move into QnCameraAdvancedParameterCondition.
static QnCameraAdvancedParameterCondition valueCondition(const QString& name, const QString& value)
{
    QnCameraAdvancedParameterCondition condition;
    condition.paramId = name;
    condition.type = QnCameraAdvancedParameterCondition::ConditionType::equal;
    condition.value = value;
    return condition;
}

// TODO: Makes sense to move into QnCameraAdvancedParameterDependency.
static QnCameraAdvancedParameterDependency valueDependency(
    QnCameraAdvancedParameterDependency::DependencyType type,
    const QString& value, const std::map<QString, QString>& valueConditions)
{
    QnCameraAdvancedParameterDependency dependency;
    dependency.id = QnUuid::createUuid().toString();
    dependency.type = type;
    dependency.range = value;
    for (const auto& keyValue: valueConditions)
        dependency.conditions.push_back(valueCondition(keyValue.first, keyValue.second));
    return dependency;
}

} // namespace

StreamCapabilityAdvancedParametersProvider::StreamCapabilityAdvancedParametersProvider(
    Camera* camera,
    const StreamCapabilityMap& capabilities,
    Qn::StreamIndex streamIndex,
    const QSize& baseResolution)
:
    m_camera(camera),
    m_streamIndex(streamIndex),
    m_descriptions(describeCapabilities(capabilities)),
    m_defaults(bestParameters(capabilities, baseResolution))
{
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
    {
        NX_VERBOSE(this, lm("Camera %1 supports %2 with %3").args(
            m_camera->getPhysicalId(), it.key(), it.value()));
    }

    if (!QJson::deserialize(camera->getProperty(proprtyName()).toUtf8(), &m_parameters))
        m_parameters = m_defaults;
}

QnLiveStreamParams StreamCapabilityAdvancedParametersProvider::getParameters() const
{
    QnMutexLocker lock(&m_mutex);
    return m_parameters;
}

bool StreamCapabilityAdvancedParametersProvider::setParameters(const QnLiveStreamParams& value)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_parameters == value)
            return true;
    }

    if (value == m_defaults)
    {
        if (!m_camera->removeProperty(proprtyName()) || !m_camera->saveParams())
            return false;
    }
    else
    {
        const auto json = QString::fromUtf8(QJson::serialized(value));
        if (!m_camera->setProperty(proprtyName(), json) || !m_camera->saveParams())
            return false;
    }

    {
        QnMutexLocker lock(&m_mutex);
        m_parameters = value;
    }

    if (qnCameraPool)
    {
        if (const auto camera = qnCameraPool->getVideoCamera(toSharedPointer(m_camera)))
        {
            const auto stream =
                (m_streamIndex == Qn::StreamIndex::primary)
                    ? camera->getPrimaryReader()
                    : camera->getSecondaryReader();

            if (stream && stream->isRunning())
                stream->pleaseReopenStream();
        }
    }

    return true;
}

QnCameraAdvancedParams StreamCapabilityAdvancedParametersProvider::descriptions()
{
    return m_descriptions;
}

QnCameraAdvancedParamValueMap StreamCapabilityAdvancedParametersProvider::get(
    const QSet<QString>& ids)
{
    QnMutexLocker lock(&m_mutex);
    QnCameraAdvancedParamValueMap values;

    if (ids.contains(parameterName(kCodec)))
        values.insert(parameterName(kCodec), m_parameters.codec);

    if (ids.contains(parameterName(kResolution)))
        values.insert(parameterName(kResolution), sizeToString(m_parameters.resolution));

    if (ids.contains(parameterName(kBitrateKpbs)))
        values.insert(parameterName(kBitrateKpbs), toString(m_parameters.bitrateKbps));

    if (ids.contains(parameterName(kFps)))
        values.insert(parameterName(kFps), toString(m_parameters.fps));

    return values;
}

QSet<QString> StreamCapabilityAdvancedParametersProvider::set(
    const QnCameraAdvancedParamValueMap& values)
{
    QSet<QString> ids;
    if (const auto value = getValue(values, kResetToDefaults))
    {
        if (setParameters(m_defaults))
            ids << parameterName(kResetToDefaults);

        return ids;
    }

    decltype(m_parameters) parameters;
    {
        QnMutexLocker lock(&m_mutex);
        parameters = m_parameters;
    }

    if (const auto value = getValue(values, kCodec))
    {
        parameters.codec = *value;
        ids.insert(parameterName(kCodec));
    }

    if (const auto value = getValue(values, kResolution))
    {
        if (auto resolution = sizeFromString(*value))
        {
            parameters.resolution = *resolution;
            ids.insert(parameterName(kResolution));
        }
    }

    if (const auto value = getValue(values, kBitrateKpbs))
    {
        const auto bitrateKbps = value->toInt();
        if (bitrateKbps > 0)
        {
            parameters.bitrateKbps = bitrateKbps;
            ids.insert(parameterName(kBitrateKpbs));
        }
    }

    if (const auto value = getValue(values, kFps))
    {
        const auto fps = value->toFloat();
        if (fps > 0)
        {
            parameters.fps = fps;
            ids.insert(parameterName(kFps));
        }
    }

    return setParameters(parameters) ? ids : QSet<QString>();
}

QnCameraAdvancedParams StreamCapabilityAdvancedParametersProvider::describeCapabilities(
    const StreamCapabilityMap& capabilities) const
{
    QMap<QString, QMap<QString, nx::media::CameraStreamCapability>> codecResolutionCapabilities;
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
        codecResolutionCapabilities[it.key().codec][sizeToString(it.key().resolution)] = it.value();

    QnCameraAdvancedParamGroup streamParameters;
    streamParameters.name = m_streamIndex == Qn::StreamIndex::primary
        ? lit("Primary") : lit("Secondary");
    streamParameters.params = describeCapabilities(codecResolutionCapabilities);

    QnCameraAdvancedParameter resetToDefaults;
    resetToDefaults.id = parameterName(kResetToDefaults);
    resetToDefaults.name = lit("Reset to Defaults");
    resetToDefaults.dataType = QnCameraAdvancedParameter::DataType::Button;
    resetToDefaults.confirmation = lit("Are you sure you want to set defaults for this stream?");
    resetToDefaults.resync = true;
    streamParameters.params.push_back(resetToDefaults);

    QnCameraAdvancedParamGroup videoStreams;
    videoStreams.name = lit("Video Streams Configuration");
    videoStreams.groups.push_back(streamParameters);

    QnCameraAdvancedParams descriptions;
    descriptions.name = proprtyName();
    descriptions.unique_id = QnUuid::createUuid().toSimpleString();
    descriptions.version = toString(this);
    descriptions.groups.push_back(videoStreams);
    return descriptions;
}

std::vector<QnCameraAdvancedParameter> StreamCapabilityAdvancedParametersProvider::describeCapabilities(
    const QMap<QString, QMap<QString, nx::media::CameraStreamCapability>>&
        codecResolutionCapabilities) const
{
    QnCameraAdvancedParameter codec;
    codec.id = parameterName(kCodec);
    codec.name = lit("Codec");
    codec.dataType = QnCameraAdvancedParameter::DataType::Enumeration;
    codec.range = makeRange(codecResolutionCapabilities.keys());

    QnCameraAdvancedParameter resolution;
    resolution.id = parameterName(kResolution);
    resolution.name = lit("Resolution");
    resolution.dataType = QnCameraAdvancedParameter::DataType::Enumeration;

    // Dependency: resolution -> codec.
    for (auto codecResolution = codecResolutionCapabilities.begin();
         codecResolution != codecResolutionCapabilities.end(); ++codecResolution)
    {
        resolution.dependencies.push_back(valueDependency(
            QnCameraAdvancedParameterDependency::DependencyType::range,
            makeRange(codecResolution.value().keys()),
            {{parameterName(kCodec), codecResolution.key()}}));
    }

    if (m_streamIndex == Qn::StreamIndex::primary)
        return {codec, resolution};

    QnCameraAdvancedParameter bitrate;
    bitrate.id = parameterName(kBitrateKpbs);
    bitrate.name = lit("Bitrate");
    bitrate.unit = lit("Kbps");
    bitrate.dataType = QnCameraAdvancedParameter::DataType::Number;
    bitrate.range = lit("1,100000");

    QnCameraAdvancedParameter fps;
    fps.id = parameterName(kFps);
    fps.name = lit("FPS");
    fps.unit = lit("Frames per Second");
    fps.dataType = QnCameraAdvancedParameter::DataType::Number;
    fps.range = lit("1,100");

    // Dependencies: bitrate -> codec + resolution; fps -> codec + resolution.
    for (auto codecResolution = codecResolutionCapabilities.begin();
         codecResolution != codecResolutionCapabilities.end(); ++codecResolution)
    {
        const auto codec = codecResolution.key();
        const auto resolutionCaps = codecResolution.value();
        for (auto resolutionCapability = codecResolution.value().begin();
             resolutionCapability != codecResolution.value().end(); ++resolutionCapability)
        {
            const auto resolution = resolutionCapability.key();
            nx::media::CameraStreamCapability capability = resolutionCapability.value();
            std::map<QString, QString> condition = {
                {parameterName(kCodec), codec},
                {parameterName(kResolution), resolution},
            };

            bitrate.dependencies.push_back(valueDependency(
                QnCameraAdvancedParameterDependency::DependencyType::range,
                lm("%1,%2").args(capability.minBitrateKbps, capability.maxBitrateKbps), condition));

            fps.dependencies.push_back(valueDependency(
                QnCameraAdvancedParameterDependency::DependencyType::range,
                lm("1,%2").args(capability.maxFps), condition));
        }
    }

    return {codec, resolution, bitrate, fps};
}

static const std::vector<QnLiveStreamParams> calculateRecomendedOptions(
    const StreamCapabilityMap& capabilities, Qn::StreamIndex streamIndex)
{
    std::map<QString, std::vector<QnLiveStreamParams>> optionsByCodec;
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
    {
        QnLiveStreamParams option;
        option.codec = it.key().codec;
        option.resolution = it.key().resolution;

        option.fps = streamIndex == Qn::StreamIndex::primary
            ? it.value().maxFps
            : QnSecurityCamResource::kDefaultSecondStreamFpsMedium;

        option.bitrateKbps = static_cast<int>(QnSecurityCamResource::rawSuggestBitrateKbps(
            Qn::QualityNormal, option.resolution, option.fps));

        optionsByCodec[option.codec].push_back(option);
    }

    // By default we recomentd to use h264 because of the performance and wide support.
    const auto h264 = optionsByCodec.find(lit("H264"));
    if (h264 != optionsByCodec.end())
        return h264->second;

    // Avoid H265 to avoid possible performance drops.
    if (optionsByCodec.size() > 1)
        optionsByCodec.erase(lit("H265"));

    return optionsByCodec.begin()->second;
}

QnLiveStreamParams StreamCapabilityAdvancedParametersProvider::bestParameters(
    const StreamCapabilityMap& capabilities, const QSize& baseResolution)
{
    const auto recomendedOptions = calculateRecomendedOptions(capabilities, m_streamIndex);
    if (m_streamIndex == Qn::StreamIndex::primary)
    {
        QnLiveStreamParams bestOption = recomendedOptions.front();
        for (const auto& option: recomendedOptions)
        {
            if (bestOption.resolution.width() * bestOption.resolution.height() <
                option.resolution.width() * option.resolution.height())
            {
                bestOption = option;
            }
        }

        NX_DEBUG(this, lm("Default is biggest resolution for primary camera %1 stream: %2")
            .args(m_camera->getPhysicalId(), QJson::serialized(bestOption)));
        return bestOption;
    }

    // For the secondary stream the option should have the same aspect ration as for
    // primary stream provided by baseResolution.
    QList<QSize> supportedResolutions;
    for (const auto& option: recomendedOptions)
        supportedResolutions.push_back(option.resolution);

    const auto selectedResolution = Camera::closestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION, Camera::getResolutionAspectRatio(baseResolution),
        UNLIMITED_RESOLUTION, supportedResolutions);

    for (const auto& option: recomendedOptions)
    {
        if (option.resolution == selectedResolution)
        {
            NX_DEBUG(this, lm("Default is closest resolution to %1 for secondary camera %2 stream: %3")
                .args(baseResolution, m_camera->getPhysicalId(), QJson::serialized(option)));
            return option;
        }
    }

    NX_ASSERT(false);
    return QnLiveStreamParams();
}

QString StreamCapabilityAdvancedParametersProvider::proprtyName() const
{
    return m_streamIndex == Qn::StreamIndex::primary
        ? lit("primaryStreamConfiguration") : lit("secondaryStreamConfiguration");
}

QString StreamCapabilityAdvancedParametersProvider::parameterName(const QString& baseName) const
{
    return lm("%1.%2").args(
        m_streamIndex == Qn::StreamIndex::primary ? lit("primaryStream") : lit("secondaryStream"),
        baseName);
}

boost::optional<QString> StreamCapabilityAdvancedParametersProvider::getValue(
    const QnCameraAdvancedParamValueMap& values,
    const QString& baseName)
{
    const auto name = parameterName(baseName);
    if (values.contains(name))
    {
        const auto value = values.value(name);
        // TODO: Consider to move this check a level higher.
        if (m_descriptions.getParameterById(name).isValueValid(value))
            return value;
    }

    return boost::none;
}

} // namespace resource
} // namespace mediaserver
} // namespace nx

