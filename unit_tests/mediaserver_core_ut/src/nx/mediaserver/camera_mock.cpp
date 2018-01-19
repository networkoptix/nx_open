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

QSet<QString> CameraMock::setApiParamiters(const QnCameraAdvancedParamValueMap& values)
{
    QSet<QString> result;
    for (const auto& id: values)
        if (setApiParameter(id, values.value(id)))
            result.insert(id);

    return result;
}

void CameraMock::setStreamCapabilityMaps(StreamCapabilityMap primary, StreamCapabilityMap secondary)
{
    m_streamCapabilityMaps[true] = std::move(primary);
    m_streamCapabilityMaps[false] = std::move(secondary);
}

static const void addParameterTo

QnCameraAdvancedParams CameraMock::makeParameterDescriptions(const std::vector<QString>& parameters)
{
    QnCameraAdvancedParams descriptions;
    for (const auto& parameter: parameters)
    {
        auto path = parameter.split('.');

        //parameter
    }

    return QnCameraAdvancedParams();
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
    return {};
}

StreamCapabilityMap CameraMock::getStreamCapabilityMapFromDrives(bool primaryStream)
{
    return m_streamCapabilityMaps.value(primaryStream);
}

QnSharedResourcePointer<CameraMock> CameraTest::newCamera(std::function<void(CameraMock*)> setup)
{
    // TODO: Use MediaServerLauncher, and make a huck for QnResourceDiscoveryManager to
    // the resource is created by API request in server's ResourcePool so real initilization
    // takes place.
    QnSharedResourcePointer<CameraMock> camera(new CameraMock());
    setup(camera.data());
    return camera->initInternal() ? camera : QnSharedResourcePointer<CameraMock>();
}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx

