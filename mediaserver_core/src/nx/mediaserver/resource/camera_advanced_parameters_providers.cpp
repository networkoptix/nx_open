#include "camera_advanced_parameters_providers.h"

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

// TODO: Makes sense to somewhere in nx::utils.
static const QString sizeToString(const QSize& size)
{
    return lm("%1x%2").args(size.width(), size.height());
}

// TODO: Makes sense to somewhere in nx::utils.
boost::optional<QSize> sizeFromString(const QString& size)
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
    bool isPrimaryStream,
    const QSize& baseResolution)
:
    m_camera(camera),
    m_isPrimaryStream(isPrimaryStream),
    m_descriptions(describeCapabilities(capabilities))
{
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
    {
        NX_VERBOSE(this, lm("Camera %1 supports %2 with %3").args(
            m_camera->getPhysicalId(), it.key(), it.value()));
    }

    if (!QJson::deserialize(camera->getProperty(proprtyName()).toUtf8(), &m_parameters))
        m_parameters = bestParameters(capabilities, baseResolution);
}

QnLiveStreamParams StreamCapabilityAdvancedParametersProvider::getParameters() const
{
    QnMutexLocker lock(&m_mutex);
    return m_parameters;
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
    QString newPropertyValue;
    QSet<QString> ids;

    {
        QnMutexLocker lock(&m_mutex);
        if (const auto value = getValue(values, kCodec))
        {
            m_parameters.codec = *value;
            ids.insert(parameterName(kCodec));
        }

        if (const auto value = getValue(values, kResolution))
        {
            if (auto resolution = sizeFromString(*value))
            {
                m_parameters.resolution = *resolution;
                ids.insert(parameterName(kResolution));
            }
        }

        if (const auto value = getValue(values, kBitrateKpbs))
        {
            const auto bitrateKbps = value->toInt();
            if (bitrateKbps > 0)
            {
                m_parameters.bitrateKbps = bitrateKbps;
                ids.insert(parameterName(kBitrateKpbs));
            }
        }

        if (const auto value = getValue(values, kFps))
        {
            const auto fps = value->toFloat();
            if (fps > 0)
            {
                m_parameters.fps = fps;
                ids.insert(parameterName(kFps));
            }
        }

        newPropertyValue = QJson::serialized(m_parameters);
    }

    if (!m_camera->setProperty(proprtyName(), newPropertyValue)
        || !m_camera->saveParams())
    {
        return QSet<QString>();
    }

    NX_DEBUG(this, lm("Camera %1, updated %2 value: %3").args(
        m_camera->getPhysicalId(), proprtyName(), newPropertyValue));

    return ids;
}

QnCameraAdvancedParams StreamCapabilityAdvancedParametersProvider::describeCapabilities(
    const StreamCapabilityMap& capabilities) const
{
    QMap<QString, QMap<QString, nx::media::CameraStreamCapability>> codecResolutionCapabilities;
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
        codecResolutionCapabilities[it.key().codec][sizeToString(it.key().resolution)] = it.value();

    QnCameraAdvancedParameter codec;
    codec.id = parameterName(kCodec);
    codec.name = lit("Codec");
    codec.dataType = QnCameraAdvancedParameter::DataType::Enumeration;
    codec.range = codecResolutionCapabilities.keys().join(',');

    QnCameraAdvancedParameter resolution;
    resolution.id = parameterName(kResolution);
    resolution.name = lit("Resolution");
    resolution.dataType = QnCameraAdvancedParameter::DataType::Enumeration;

    QnCameraAdvancedParameter bitrate;
    bitrate.id = parameterName(kBitrateKpbs);
    bitrate.name = lit("Bitrate (Kbps)");
    bitrate.dataType = QnCameraAdvancedParameter::DataType::Number;
    bitrate.range = lit("1,999999");

    QnCameraAdvancedParameter fps;
    fps.id = parameterName(kBitrateKpbs);
    fps.name = lit("Frame per Second (FPS)");
    fps.dataType = QnCameraAdvancedParameter::DataType::Number;
    fps.range = lit("1,999999");

    // Dependency: resolution -> codec.
    for (auto codecResolution = codecResolutionCapabilities.begin();
         codecResolution != codecResolutionCapabilities.end(); ++codecResolution)
    {
        resolution.dependencies.push_back(valueDependency(
            QnCameraAdvancedParameterDependency::DependencyType::range,
            codecResolution.value().keys().join(','),
            {{parameterName(kCodec), codecResolution.key()}}));
    }

    // Dependency: bitrate -> codec + resolution; fps -> codec + resolution.
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
                lm("%1,%2").args(capability.defaultFps, capability.maxFps), condition));
        }
    }

    QnCameraAdvancedParamGroup streamParameters;
    streamParameters.name = m_isPrimaryStream ?
        lit("Primary (High Quality)") : lit("Secondary (Low Quality)");

    streamParameters.params.push_back(codec);
    streamParameters.params.push_back(resolution);
    streamParameters.params.push_back(bitrate);
    streamParameters.params.push_back(fps);

    QnCameraAdvancedParamGroup videoStreams;
    videoStreams.name = lit("Video Stream Configuration");
    videoStreams.groups.push_back(streamParameters);

    QnCameraAdvancedParams descriptions;
    descriptions.name = proprtyName();
    descriptions.unique_id = QnUuid::createUuid().toSimpleString();
    descriptions.version = toString(this);
    descriptions.groups.push_back(videoStreams);
    return descriptions;
}

QnLiveStreamParams StreamCapabilityAdvancedParametersProvider::bestParameters(
    const StreamCapabilityMap& capabilities, const QSize& baseResolution)
{
    // By default we recomentd to use h264 because of the performance and wide support.
    std::vector<QnLiveStreamParams> recomendedOptions;
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
    {
        if (!it.key().codec.contains(lit("264")))
            continue;

        QnLiveStreamParams option;
        option.codec = it.key().codec;
        option.resolution = it.key().resolution;
        option.bitrateKbps = it.value().maxBitrateKbps;
        option.fps = it.value().maxFps;
        recomendedOptions.push_back(option);
    }

    if (!recomendedOptions.empty())
    {
        if (m_isPrimaryStream)
        {
            // Biggest resolution is the best option for primary stream.
            QnLiveStreamParams bestOption = recomendedOptions.front();
            for (const auto& option: recomendedOptions)
            {
                if (bestOption.resolution.width() * bestOption.resolution.height() <
                    option.resolution.width() * option.resolution.height())
                {
                    bestOption = option;
                }
            }

            return bestOption;
        }

        // For the secondary stream the option should have the same aspect ration as for
        // primary stream provided by baseResolution.
        QList<QSize> supportedResolutions;
        for (const auto& option: recomendedOptions)
            supportedResolutions.push_back(option.resolution);

        const auto selectedResolution = nx::mediaserver::resource::Camera::getNearestResolution(
            baseResolution,
            nx::mediaserver::resource::Camera::getResolutionAspectRatio(baseResolution),
            baseResolution.width() * baseResolution.height(),
            supportedResolutions);

        for (const auto& option: recomendedOptions)
        {
            if (option.resolution == selectedResolution)
                return option;
        }
    }

    QnLiveStreamParams bestOption;
    bestOption.codec = capabilities.firstKey().codec;
    bestOption.resolution = capabilities.firstKey().resolution;
    bestOption.bitrateKbps = capabilities.first().maxBitrateKbps;
    bestOption.fps = capabilities.first().maxFps;

    NX_WARNING(this, lm("No good default option for camera %1, select first avaliable: %2")
        .args(m_camera->getPhysicalId(), QJson::serialized(bestOption)));

    return bestOption;
}

QString StreamCapabilityAdvancedParametersProvider::proprtyName() const
{
    return m_isPrimaryStream ? lit("pramaryStreamConfiguration") : lit("secondaryStreamConfiguration");
}

QString StreamCapabilityAdvancedParametersProvider::parameterName(const QString& baseName) const
{
    return lm("%1.%2").args(
        m_isPrimaryStream ? lit("primaryStream") : lit("secondaryStream"), baseName);
}

boost::optional<QString> StreamCapabilityAdvancedParametersProvider::getValue(
    const QnCameraAdvancedParamValueMap& values,
    const QString& baseName)
{
    const auto name = parameterName(baseName);
    if (values.contains(name))
    {
        const auto value = values.value(name);
        const auto range = m_descriptions.getParameterById(name).getRange();
        if (range.isEmpty() || range.contains(value))
            return value;
    }

    return boost::none;
}

} // namespace resource
} // namespace mediaserver
} // namespace nx

