#pragma once

#include <gtest/gtest.h>
#include <nx/mediaserver/resource/camera.h>
#include <nx/utils/std/cpp14.h>
#include <core/dataprovider/data_provider_factory.h>

class QnDataProviderFactory;

namespace nx {
namespace mediaserver {
namespace resource {
namespace test {

class CameraMock: public Camera
{
    Q_OBJECT
public:
    CameraMock();

    CameraDiagnostics::Result initialize();

    template<template<typename> class ApiProvider>
    void makeApiAdvancedParametersProvider(const std::vector<QString>& parameters);

    boost::optional<QString> getApiParameter(const QString& id);
    bool setApiParameter(const QString& id, const QString& value);

    QnCameraAdvancedParamValueMap getApiParameters(const QSet<QString>& ids);
    QSet<QString> setApiParameters(const QnCameraAdvancedParamValueMap& values);

    void setStreamCapabilityMaps(StreamCapabilityMap primary, StreamCapabilityMap secondary);
    void setMediaTraits(nx::media::CameraTraits traits);
    void enableSetProperty(bool isEnabled);

    static QnCameraAdvancedParams makeParameterDescriptions(
        const std::vector<QString>& parameters);

    void setPtzController(QnAbstractPtzController* controller);

    virtual bool isCameraControlDisabled() const override;
    virtual Qn::MotionType getMotionType() const override;
    virtual bool hasDualStreamingInternal() const override;
    virtual bool hasDualStreaming() const override;

    virtual QString getProperty(const QString& key) const override;
    virtual bool setProperty(
        const QString& key,
        const QString& value,
        PropertyOptions options) override;

    virtual bool saveParams() override;

protected:
    virtual QString getDriverName() const override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual std::vector<AdvancedParametersProvider*> advancedParametersProviders() override;
    virtual StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual nx::media::CameraTraits mediaTraits() const override;
    virtual QnAbstractPtzController* createPtzControllerInternal() const override;

private:
    friend class CameraTest;
    std::map<QString, QString> m_apiAadvancedParameters;
    std::vector<std::unique_ptr<AdvancedParametersProvider>> m_advancedParametersProviders;
    StreamCapabilityMaps m_streamCapabilityMaps;
    nx::media::CameraTraits m_mediaTraits;
    bool isSetProprtyEnabled = true;
    mutable std::map<QString, QString> m_properties;
    QnAbstractPtzController* m_ptzController;
};

class CameraTest: public testing::Test
{
public:
    static QnSharedResourcePointer<CameraMock> newCamera(std::function<void(CameraMock*)> setup);

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;
    QnDataProviderFactory* dataProviderFactory() const;

private:
    QScopedPointer<QnDataProviderFactory> m_dataProviderFactory;
};

// -------------------------------------------------------------------------------------------------

template<template<typename> class ApiProvider>
void CameraMock::makeApiAdvancedParametersProvider(
    const std::vector<QString>& parameters)
{
    auto provider = std::make_unique<ApiProvider<CameraMock>>(this);
    provider->assign(makeParameterDescriptions(parameters));

    for (const auto& id: provider->descriptions().allParameterIds())
        m_apiAadvancedParameters[id] = lit("default");

    m_advancedParametersProviders.push_back(std::move(provider));
}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx
