#pragma once

#include <gtest/gtest.h>
#include <nx/mediaserver/resource/camera.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver {
namespace resource {
namespace test {

class CameraMock: public Camera
{
public:
    template<template<typename> class ApiProvider>
    void makeApiAdvancedParametersProvider(const std::vector<QString>& parameters);

    boost::optional<QString> getApiParameter(const QString& id);
    bool setApiParameter(const QString& id, const QString& value);

    QnCameraAdvancedParamValueMap getApiParamiters(const QSet<QString>& ids);
    QSet<QString> setApiParamiters(const QnCameraAdvancedParamValueMap& values);

    void setStreamCapabilityMaps(StreamCapabilityMap primary, StreamCapabilityMap secondary);

    static QnCameraAdvancedParams makeParameterDescriptions(const std::vector<QString>& parameters);

protected:
    virtual QString getDriverName() const override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual std::vector<AdvancedParametersProvider*> advancedParametersProviders() override;
    virtual StreamCapabilityMap getStreamCapabilityMapFromDrives(bool primaryStream) override;

private:
    friend class CameraTest;
    std::map<QString, QString> m_apiAadvancedParameters;
    std::vector<std::unique_ptr<AdvancedParametersProvider>> m_advancedParametersProviders;
    QMap<bool, StreamCapabilityMap> m_streamCapabilityMaps;
};

class CameraTest: public testing::Test
{
public:
    static QnSharedResourcePointer<CameraMock> newCamera(std::function<void(CameraMock*)> setup);
};

// -------------------------------------------------------------------------------------------------

template<template<typename> class ApiProvider>
void CameraMock::makeApiAdvancedParametersProvider(
    const std::vector<QString>& parameters)
{
    auto provider = std::make_unique<ApiProvider<CameraMock>>(this);
    provider->assign(makeParameterDescriptions(parameters));
    m_advancedParametersProviders.push_back(std::move(provider));
}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx
