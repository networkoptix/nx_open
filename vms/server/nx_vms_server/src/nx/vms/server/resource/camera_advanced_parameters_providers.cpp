#include "camera_advanced_parameters_providers.h"

#include <boost/rational.hpp>

#include <camera/camera_pool.h>
#include <camera/video_camera.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace vms::server {
namespace resource {

namespace {

static const auto kCodec = lit("codec");
static const auto kResolution = lit("resolution");
static const auto kBitrateKpbs = lit("bitrateKbps");
static const auto kFps = lit("fps");
static const auto kResetToDefaults = lit("resetToDefaults");
static const auto kStreamParametersProviderId = lit("971009D8-384D-43C1-85BF-44A2B99253D5");
static const auto kStreamParametersProviderVersion = lit("1.0");
static const double kDefaultAllowedAspectRatioDiff = 0.1;
static const QString kAllowedAspectRatioDiffAttribute = lit("allowedAspectRatioDiff");

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
static QnCameraAdvancedParameterCondition condition(
    const QString& name,
    const QString& value,
    QnCameraAdvancedParameterCondition::ConditionType conditionType)
{
    QnCameraAdvancedParameterCondition condition;
    condition.paramId = name;
    condition.type = conditionType;
    condition.value = value;
    return condition;
}

// TODO: Makes sense to move into QnCameraAdvancedParameterCondition.
static QnCameraAdvancedParameterCondition valueCondition(const QString& name, const QString& value)
{
    return condition(name, value, QnCameraAdvancedParameterCondition::ConditionType::equal);
}

static QnCameraAdvancedParameterDependency dependency(
    QnCameraAdvancedParameterDependency::DependencyType type,
    const QString& value,
    std::vector<QnCameraAdvancedParameterCondition> conditions,
    const QString& prefix = QString())
{
    QnCameraAdvancedParameterDependency dependency;
    dependency.type = type;
    dependency.range = value;
    dependency.conditions = std::move(conditions);
    dependency.autoFillId(prefix);
    return dependency;
}

// TODO: Makes sense to move into QnCameraAdvancedParameterDependency.
static QnCameraAdvancedParameterDependency valueDependency(
    QnCameraAdvancedParameterDependency::DependencyType type,
    const QString& value,
    const std::map<QString, QString>& valueConditions)
{
    QnCameraAdvancedParameterDependency dependency;
    dependency.type = type;
    dependency.range = value;
    for (const auto& keyValue: valueConditions)
        dependency.conditions.push_back(valueCondition(keyValue.first, keyValue.second));
    dependency.autoFillId();
    return dependency;
}

static boost::optional<boost::rational<int>> resolutionAspectRatio(const QString& resolutionString)
{
    const auto resolution = sizeFromString(resolutionString);
    if (resolution == boost::none)
        return boost::none;

    return boost::rational<int>(resolution->width(), resolution->height());
}

static std::function<bool(const QString&)> hasSimilarAspectRatioFilter(
    double aspectRatio,
    double allowedAspectRatioDifference)
{
    return
        [aspectRatio, allowedAspectRatioDifference](const QString& resolutionString)
        {
            const auto aspectFromResolution = resolutionAspectRatio(resolutionString);
            if (aspectFromResolution == boost::none)
                return false;

            const auto diff = aspectRatio - boost::rational_cast<double>(*aspectFromResolution);
            return diff < allowedAspectRatioDifference && diff > -allowedAspectRatioDifference;
        };
}

} // namespace

StreamCapabilityAdvancedParametersProvider::StreamCapabilityAdvancedParametersProvider(
    Camera* camera,
    const StreamCapabilityMaps& capabilities,
    const nx::media::CameraTraits& traits,
    MotionStreamType streamIndex,
    const QSize& baseResolution)
:
    m_camera(camera),
    m_capabilities(capabilities),
    m_traits(traits),
    m_streamIndex(streamIndex),
    m_descriptions(describeCapabilities()),
    m_defaults(bestParameters(capabilities[streamIndex], baseResolution))
{
    for (auto it = capabilities[m_streamIndex].begin(); it != capabilities[m_streamIndex].end(); ++it)
    {
        NX_VERBOSE(this, lm("Camera %1 supports %2 with %3").args(
            m_camera->getPhysicalId(), it.key(), it.value()));
    }

    if (!QJson::deserialize(camera->getProperty(proprtyName()).toUtf8(), &m_parameters))
        m_parameters = m_defaults;
    updateMediaCapabilities();
    m_camera->saveProperties();
}

QnLiveStreamParams StreamCapabilityAdvancedParametersProvider::getParameters() const
{
    QnMutexLocker lock(&m_mutex);
    return m_parameters;
}

bool StreamCapabilityAdvancedParametersProvider::setParameters(const QnLiveStreamParams& value)
{
    QnMutexLocker lock(&m_mutex);
    if (m_parameters == value)
        return true;

    if (value == m_defaults)
    {
        if (!m_camera->setProperty(proprtyName(), QString()))
            return false;
    }
    else
    {
        const auto json = QString::fromUtf8(QJson::serialized(value));
        if (!m_camera->setProperty(proprtyName(), json))
            return false;
    }

    m_parameters = value;

    auto videoCameraPool = m_camera->serverModule()->videoCameraPool();
    if (videoCameraPool)
    {
        if (const auto camera = videoCameraPool->getVideoCamera(toSharedPointer(m_camera)))
        {
            const auto stream =
                (m_streamIndex == MotionStreamType::primary)
                ? camera->getPrimaryReader()
                : camera->getSecondaryReader();

            if (stream && stream->isRunning())
                stream->pleaseReopenStream();
        }
    }
    updateMediaCapabilities();
    return m_camera->saveProperties();
}

void StreamCapabilityAdvancedParametersProvider::updateMediaCapabilities()
{
    StreamCapabilityKey key;
    key.codec = m_parameters.codec;
    key.resolution = m_parameters.resolution;
    auto streamCapabilities = currentStreamCapabilities().value(key);

    auto mediaCapabilities = m_camera->cameraMediaCapability();
    mediaCapabilities.streamCapabilities[m_streamIndex] = streamCapabilities;
    m_camera->setCameraMediaCapability(mediaCapabilities);
}

QSet<QString> StreamCapabilityAdvancedParametersProvider::filterResolutions(
    MotionStreamType streamIndex,
    std::function<bool(const QString&)> filterFunc) const
{
    const auto& capabilities = m_capabilities[streamIndex];
    QSet<QString> result;

    for (auto codecItr = capabilities.cbegin(); codecItr != capabilities.cend(); ++codecItr)
    {
        const auto resolutionString = sizeToString(codecItr.key().resolution);
        if (filterFunc(resolutionString))
            result.insert(resolutionString);
    }

    return result;
}

boost::optional<nx::media::CameraTraitAttributes> StreamCapabilityAdvancedParametersProvider::trait(
    nx::media::CameraTraitType traitType) const
{
    const auto traitString = QnLexical::serialized(traitType);
    auto traitItr = m_traits.find(traitString);
    if (traitItr == m_traits.cend())
        return boost::none;

    return traitItr->second;
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

const StreamCapabilityMap&
    StreamCapabilityAdvancedParametersProvider::currentStreamCapabilities() const
{
    return m_capabilities.find(m_streamIndex).value();
}

QnCameraAdvancedParams StreamCapabilityAdvancedParametersProvider::describeCapabilities() const
{
    QMap<QString, QMap<QString, nx::media::CameraStreamCapability>> codecResolutionCapabilities;
    const auto streamCapabilities = currentStreamCapabilities();

    auto rpt = streamCapabilities.begin();
    for (auto it = streamCapabilities.begin(); it != streamCapabilities.end(); ++it)
        codecResolutionCapabilities[it.key().codec][sizeToString(it.key().resolution)] = it.value();

    QnCameraAdvancedParamGroup streamParameters;
    streamParameters.name = m_streamIndex == MotionStreamType::primary
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
    descriptions.unique_id = kStreamParametersProviderId;
    descriptions.version = kStreamParametersProviderVersion;
    descriptions.groups.push_back(videoStreams);
    return descriptions;
}

std::vector<QnCameraAdvancedParameter> StreamCapabilityAdvancedParametersProvider::describeCapabilities(
    const QMap<CodecString, QMap<ResolutionString, nx::media::CameraStreamCapability>>&
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

    const auto traitAttributes = trait(nx::media::CameraTraitType::aspectRatioDependent);
    const bool doesResolutionDependOnAspectRatio = m_streamIndex == MotionStreamType::secondary
        && traitAttributes != boost::none;

    auto allowedAspectRatioDiff = kDefaultAllowedAspectRatioDiff;
    if (doesResolutionDependOnAspectRatio)
    {
        for (const auto& entry: *traitAttributes)
        {
            const auto& attributeName = entry.first;
            const auto& attributeValue = entry.second;

            if (attributeName != kAllowedAspectRatioDiffAttribute)
                continue;

            allowedAspectRatioDiff = attributeValue.toDouble();
        }
    }

    for (auto codecResolution = codecResolutionCapabilities.begin();
         codecResolution != codecResolutionCapabilities.end(); ++codecResolution)
    {
        if (!doesResolutionDependOnAspectRatio)
        {
            resolution.dependencies.push_back(valueDependency(
                QnCameraAdvancedParameterDependency::DependencyType::range,
                makeRange(codecResolution.value().keys()),
                { {parameterName(kCodec), codecResolution.key()} }));
        }
        else
        {
            const auto resolutionsRestrictedByCodec = codecResolution.value().keys().toSet();
            std::set<boost::rational<int>> processedAspects;
            for (auto resolutionCapability = codecResolution.value().cbegin();
                resolutionCapability != codecResolution.value().cend();
                ++resolutionCapability)
            {
                const auto& resolutionString = resolutionCapability.key();
                const auto aspectRatio = resolutionAspectRatio(resolutionString);
                if (aspectRatio == boost::none)
                    continue;

                if (processedAspects.find(*aspectRatio) != processedAspects.cend())
                    continue;

                processedAspects.insert(*aspectRatio);
                const auto filter = hasSimilarAspectRatioFilter(
                    boost::rational_cast<double>(*aspectRatio),
                    allowedAspectRatioDiff);

                const auto primaryResolutionsRestrictedByAspectRatio = filterResolutions(
                    MotionStreamType::primary,
                    filter);

                auto secondaryResolutionsRestrictedByAspectRatio = filterResolutions(
                    MotionStreamType::secondary,
                    filter);

                const auto availableSecondaryResolutionRange = makeRange(
                    secondaryResolutionsRestrictedByAspectRatio.intersect(
                        resolutionsRestrictedByCodec).toList());

                resolution.dependencies.push_back(
                    dependency(
                        QnCameraAdvancedParameterDependency::DependencyType::range,
                        availableSecondaryResolutionRange,
                        {
                            QnCameraAdvancedParameterCondition(
                                QnCameraAdvancedParameterCondition::ConditionType::equal,
                                parameterName(kCodec),
                                codecResolution.key()),
                            QnCameraAdvancedParameterCondition(
                                QnCameraAdvancedParameterCondition::ConditionType::inRange,
                                streamParameterName(MotionStreamType::primary, kResolution),
                                primaryResolutionsRestrictedByAspectRatio.toList().join(L','))
                        },
                        lit("aspectRatio")));
            }
        }
    }

    if (m_streamIndex == MotionStreamType::primary)
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
    const StreamCapabilityMap& capabilities,
    nx::vms::api::MotionStreamType streamIndex,
    Camera* const camera)
{
    std::map<QString, std::vector<QnLiveStreamParams>> optionsByCodec;
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
    {
        QnLiveStreamParams option;
        option.codec = it.key().codec;
        option.resolution = it.key().resolution;

        if (streamIndex == nx::vms::api::MotionStreamType::primary)
        {
            option.fps = it.value().maxFps;
        }
        else
        {
            static const int kIsBigSecondaryResolution = 720 * 480;
            const bool secondaryResolutionIsLarge =
                option.resolution.width() * option.resolution.height() >= kIsBigSecondaryResolution;

            // Setup low fps if secondary resolution is large and there is no Hardware motion detector to save CPU usage
            option.fps = secondaryResolutionIsLarge
                && !camera->supportedMotionType().testFlag(Qn::MotionType::MT_HardwareGrid)
                    ? QnSecurityCamResource::kDefaultSecondStreamFpsLow
                    : QnSecurityCamResource::kDefaultSecondStreamFpsMedium;
        }

        option.bitrateKbps = static_cast<int>(QnSecurityCamResource::rawSuggestBitrateKbps(
            Qn::StreamQuality::normal, option.resolution, option.fps));

        optionsByCodec[option.codec].push_back(option);
    }

    // By default we recommend to use h264 because of the performance and wide support.
    const auto bestCodec = optionsByCodec.find(camera->defaultCodec());
    if (bestCodec != optionsByCodec.end())
        return bestCodec->second;

    // Remove H265 to avoid possible performance drops.
    if (optionsByCodec.size() > 1)
        optionsByCodec.erase(lit("H265"));

    return optionsByCodec.begin()->second;
}

QnLiveStreamParams StreamCapabilityAdvancedParametersProvider::bestParameters(
    const StreamCapabilityMap& capabilities, const QSize& baseResolution)
{
    const auto recomendedOptions = calculateRecomendedOptions(capabilities, m_streamIndex, m_camera);
    if (m_streamIndex == MotionStreamType::primary)
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

    QSize selectedResolution = Camera::closestSecondaryResolution(
        Camera::getResolutionAspectRatio(baseResolution),
        supportedResolutions);

    for (const auto& option: recomendedOptions)
    {
        if (option.resolution == selectedResolution)
        {
            NX_DEBUG(this, lm("Default is closest resolution to %1 for secondary camera %2 stream: %3")
                .args(baseResolution, m_camera->getPhysicalId(), QJson::serialized(option)));
            return option;
        }
    }

    if (!recomendedOptions.empty())
        return recomendedOptions.front();

    NX_ASSERT(false);
    return QnLiveStreamParams();
}

QString StreamCapabilityAdvancedParametersProvider::proprtyName() const
{
    return m_streamIndex == MotionStreamType::primary
        ? lit("primaryStreamConfiguration") : lit("secondaryStreamConfiguration");
}

QString StreamCapabilityAdvancedParametersProvider::parameterName(const QString& baseName) const
{
    return streamParameterName(m_streamIndex, baseName);
}

QString StreamCapabilityAdvancedParametersProvider::streamParameterName(
    MotionStreamType stream,
    const QString & baseName)
{
    return lm("%1.%2").args(
        stream == MotionStreamType::primary ? lit("primaryStream") : lit("secondaryStream"),
        baseName);
    return QString();
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
} // namespace vms::server
} // namespace nx
