#include "camera_mock.h"

namespace nx {
namespace mediaserver {
namespace resource {
namespace test {

boost::optional<QString> CameraMock::getApiParameter(const QString& id)
{
    const auto it = m_apiAadvancedParameters.find(id);
    if (it == m_apiAadvancedParameters.end())
        return boost::none;

    return it->second;
}

bool CameraMock::setApiParameter(const QString& id, const QString& value)
{
    const auto it = m_apiAadvancedParameters.find(id);
    if (it == m_apiAadvancedParameters.end())
        return false;

    it->second = value;
    return true;
}

QnCameraAdvancedParamValueMap CameraMock::getApiParamiters(const QSet<QString>& ids)
{
    QnCameraAdvancedParamValueMap result;
    for (const auto& id: ids)
        if (auto value = getApiParameter(id))
            result.insert(id, *value);

    return result;
}

QSet<QString> CameraMock::setApiParameters(const QnCameraAdvancedParamValueMap& values)
{
    QSet<QString> result;
    for (auto it = values.begin(); it != values.end(); ++it)
        if (setApiParameter(it.key(), it.value()))
            result.insert(it.key());

    return result;
}

void CameraMock::setStreamCapabilityMaps(StreamCapabilityMap primary, StreamCapabilityMap secondary)
{
    m_streamCapabilityMaps[Qn::StreamIndex::primary] = std::move(primary);
    m_streamCapabilityMaps[Qn::StreamIndex::secondary] = std::move(secondary);
}

void CameraMock::enableSetProperty(bool isEnabled)
{
    isSetProprtyEnabled = isEnabled;
}

static void addParameterToGroups(
    std::vector<QnCameraAdvancedParamGroup>* groups,
    QStringList parameterPath, const QString& parameterName)
{
    const auto groupName = parameterPath.front();
    parameterPath.pop_front();

    auto groupIt = std::find_if(groups->begin(), groups->end(),
        [&](const QnCameraAdvancedParamGroup& group) { return group.name == groupName; });

    if (groupIt == groups->end())
    {
        QnCameraAdvancedParamGroup group;
        group.name = groupName;
        groupIt = groups->insert(groups->end(), group);
    }

    if (!parameterPath.empty())
        return addParameterToGroups(&groupIt->groups, std::move(parameterPath), parameterName);

    QnCameraAdvancedParameter parameter;
    parameter.id = parameterName;
    parameter.name = parameterName;
    parameter.dataType = QnCameraAdvancedParameter::DataType::String;
    groupIt->params.push_back(std::move(parameter));
}

QnCameraAdvancedParams CameraMock::makeParameterDescriptions(const std::vector<QString>& parameters)
{
    QnCameraAdvancedParams descriptions;
    descriptions.name = QString::fromUtf8(__func__);
    descriptions.version = lit("1.0");
    descriptions.unique_id = lit("X");
    for (const auto& name: parameters)
    {
        auto path = name.split('.');
        NX_CRITICAL(path.size() >= 2);
        path.pop_back();
        addParameterToGroups(&descriptions.groups, path, name);
    }

    return descriptions;
}

QString CameraMock::getDriverName() const
{
    return lit("mock");
}

QnAbstractStreamDataProvider* CameraMock::createLiveDataProvider()
{
    return nullptr;
}

CameraDiagnostics::Result CameraMock::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}

std::vector<Camera::AdvancedParametersProvider*> CameraMock::advancedParametersProviders()
{
    std::vector<AdvancedParametersProvider*> rawPointers;
    for (const auto& provider: m_advancedParametersProviders)
        rawPointers.push_back(provider.get());

    return rawPointers;
}

StreamCapabilityMap CameraMock::getStreamCapabilityMapFromDrives(Qn::StreamIndex streamIndex)
{
    return m_streamCapabilityMaps.value(streamIndex);
}

QString CameraMock::getProperty(const QString& key) const
{
    return m_properties[key];
}

bool CameraMock::setProperty(const QString& key, const QString& value, PropertyOptions /*options*/)
{
    if (!isSetProprtyEnabled)
        return false;

    m_properties[key] = value;
    return true;
}

bool CameraMock::saveParams()
{
    return true;
}

QnSharedResourcePointer<CameraMock> CameraTest::newCamera(std::function<void(CameraMock*)> setup)
{
    QnSharedResourcePointer<CameraMock> camera(new CameraMock());
    setup(camera.data());
    return camera->initInternal() ? camera : QnSharedResourcePointer<CameraMock>();
}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx

