#pragma once

#include <nx/mediaserver/resource/camera.h>

namespace nx {
namespace mediaserver {
namespace resource {

/**
 * Base Camera::AdvancedParametersProvider implementation helper.
 */
template<typename ApiProvider>
class ApiAdvancedParamitersProvider: public Camera::AdvancedParametersProvider
{
public:
    ApiAdvancedParamitersProvider(ApiProvider* api);
    virtual QnCameraAdvancedParams descriptions() override;

    void assign(QnCameraAdvancedParams values);
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    void clear();

protected:
    ApiProvider* const m_api;
    mutable QnMutex m_mutex;
    QnCameraAdvancedParams m_parameters;
};

/**
 * Automatic get/set redirector to resource getApiParameters/setApiParameters.
 */
template<typename ApiProvider>
class ApiMultiAdvancedParamitersProvider: public ApiAdvancedParamitersProvider<ApiProvider>
{
public:
    using Base = ApiAdvancedParamitersProvider<ApiProvider>;
    using Base::Base;

    virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) override;
    virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) override;
};

/**
 * Automatic get/set redirector to resource getApiParameter/setApiParameter.
 */
template<typename ApiProvider>
class ApiSingleAdvancedParamitersProvider: public ApiAdvancedParamitersProvider<ApiProvider>
{
public:
    using Base = ApiAdvancedParamitersProvider<ApiProvider>;
    using Base::Base;

    virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) override;
    virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) override;
};

/**
 * Provides codec, resolution, bitrate and FPS options for primary or secondary stream.
 * Stores selected values in a designated property.
 *
 * Parameters structure:
 * Group: Video Stream Configuration
 *   + Group: <STREAM>
 *     + Parametrs: Codec, resolution, bitrate and FPS
 */
class StreamCapabilityAdvancedParametersProvider: public Camera::AdvancedParametersProvider
{
public:
    StreamCapabilityAdvancedParametersProvider(
        Camera* camera,
        const StreamCapabilityMap& capabilities,
        Qn::StreamIndex streamIndex,
        const QSize& baseResolution);

    QnLiveStreamParams getParameters() const;

    virtual QnCameraAdvancedParams descriptions() override;
    virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) override;
    virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) override;

private:
    QnCameraAdvancedParams describeCapabilities(const StreamCapabilityMap& capabilities) const;
    std::vector<QnCameraAdvancedParameter> describeCapabilities(
        const QMap<QString, QMap<QString, nx::media::CameraStreamCapability>>&
            codecResolutionCapabilities) const;

    QnLiveStreamParams bestParameters(
        const StreamCapabilityMap& capabilities, const QSize& baseResolution);

    QString proprtyName() const;
    QString parameterName(const QString& baseName) const;
    boost::optional<QString> getValue(
        const QnCameraAdvancedParamValueMap& values,
        const QString& baseName);

private:
    Camera* const m_camera;
    const Qn::StreamIndex m_streamIndex;
    const QnCameraAdvancedParams m_descriptions;

    mutable QnMutex m_mutex;
    QnLiveStreamParams m_parameters;
};

// -------------------------------------------------------------------------------------------------

template<typename ApiProvider>
ApiAdvancedParamitersProvider<ApiProvider>::ApiAdvancedParamitersProvider(ApiProvider* api)
    : m_api(api)
{
}

template<typename ApiProvider>
QnCameraAdvancedParams ApiAdvancedParamitersProvider<ApiProvider>::descriptions()
{
    QnMutexLocker lock(&m_mutex);
    return m_parameters;
}

template<typename ApiProvider>
void ApiAdvancedParamitersProvider<ApiProvider>::assign(QnCameraAdvancedParams values)
{
    QnMutexLocker lock(&m_mutex);
    m_parameters = std::move(values);
}

template<typename ApiProvider>
QnCameraAdvancedParameter ApiAdvancedParamitersProvider<ApiProvider>::getParameterById(
    const QString &id) const
{
    QnMutexLocker lock(&m_mutex);
    return m_parameters.getParameterById(id);
}

template<typename ApiProvider>
void ApiAdvancedParamitersProvider<ApiProvider>::clear()
{
    QnMutexLocker lock(&m_mutex);
    m_parameters.clear();
}

template<typename ApiProvider>
QnCameraAdvancedParamValueMap ApiMultiAdvancedParamitersProvider<ApiProvider>::get(
    const QSet<QString>& ids)
{
    return this->m_api->getApiParamiters(ids);
}

template<typename ApiProvider>
QSet<QString> ApiMultiAdvancedParamitersProvider<ApiProvider>::set(
    const QnCameraAdvancedParamValueMap& values)
{
    return this->m_api->setApiParameters(values);
}

template<typename ApiProvider>
QnCameraAdvancedParamValueMap ApiSingleAdvancedParamitersProvider<ApiProvider>::get(
    const QSet<QString>& ids)
{
    QnCameraAdvancedParamValueMap values;
    for (const auto& id: ids)
    {
        if (auto value = this->m_api->getApiParameter(id))
            values.insert(id, std::move(*value));
    }

    return values;
}

template<typename ApiProvider>
QSet<QString> ApiSingleAdvancedParamitersProvider<ApiProvider>::set(
    const QnCameraAdvancedParamValueMap& values)
{
    QSet<QString> ids;
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        if (this->m_api->setApiParameter(it.key(), it.value()))
            ids.insert(it.key());
    }

    return ids;
}

} // namespace resource
} // namespace mediaserver
} // namespace nx
