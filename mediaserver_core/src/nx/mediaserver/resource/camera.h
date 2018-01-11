#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource/camera_advanced_param.h>

namespace nx {
namespace mediaserver {
namespace resource {

class Camera: public QnVirtualCameraResource
{
    Q_OBJECT

public:
    static const float kMaxEps;

    Camera(QnCommonModule* commonModule = nullptr);

    /**
     * The difference between desired and real is that camera can have multiple clients we do not
     * know about or big exposure time.
     */
    int getPrimaryStreamRealFps() const;

    virtual void setUrl(const QString &url) override;
    virtual int getChannel() const override;

    /** Returns id-value pairs. */
    QnCameraAdvancedParamValueMap getAdvancedParameters(const QSet<QString>& ids);
    boost::optional<QString> getAdvancedParameter(const QString& id);

    /** Returns ids of successfully set parameters. */
    QSet<QString> setAdvancedParameters(const QnCameraAdvancedParamValueMap& values);
    bool setAdvancedParameter(const QString& id, const QString& value);

    /** Gets advanced paramiters async, handler is called when it's done. */
    void getAdvancedParametersAsync(
        const QSet<QString>& ids,
        std::function<void(const QnCameraAdvancedParamValueMap&)> handler = nullptr);

    /** Sets advanced paramiters async, handler is called when it's done. */
    void setAdvancedParametersAsync(
        const QnCameraAdvancedParamValueMap& values,
        std::function<void(const QSet<QString>&)> handler = nullptr);

    static float getResolutionAspectRatio(const QSize& resolution); // find resolution helper function

    /**
     * @param resolution Resolution for which we want to find the closest one.
     * @param aspectRatio Ideal aspect ratio for resolution we want to find. If 0 than this
     *  this parameter is not taken in account.
     * @param maxResolutionArea Maximum area of found resolution.
     * @param resolutionList List of resolutions from which we should choose closest
     * @param outCoefficient Output parameter. Measure of similarity for original resolution
     *  and a found one.
     * @return Closest resolution or empty QSize if nothing found.
     */
    static QSize getNearestResolution(
        const QSize& resolution,
        float aspectRatio,
        double maxResolutionArea,
        const QList<QSize>& resolutionList,
        double* outCoefficient = 0);

    /**
     * Simple wrapper for getNearestResolution(). Calls getNearestResolution() twice.
     * First time it calls it with provided aspectRatio. If appropriate resolution is not found
     * getNearestResolution() is called with 0 aspectRatio.
     */
    static QSize closestResolution(
        const QSize& idealResolution,
        float aspectRatio,
        const QSize& maxResolution,
        const QList<QSize>& resolutionList,
        double* outCoefficient = 0);

    class AdvancedParametersProvider
    {
    public:
        virtual ~AdvancedParametersProvider() = default;

        /** Returns supported parameters descriptions. */
        virtual QnCameraAdvancedParams descriptions() = 0;

        /** Returns id-value pairs. */
        virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) = 0;

        /** Returns ids of successfully set parameters. */
        virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) = 0;
    };

protected:
    /** Is called during initInternal(). */
    virtual CameraDiagnostics::Result initializeCameraDriver() = 0;

    /** Override to add support for advanced paramiters. */
    virtual std::vector<AdvancedParametersProvider*>  advancedParametersProviders();

private:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    int m_channelNumber; // video/audio source number
    QElapsedTimer m_lastInitTime;
    QAuthenticator m_lastCredentials;
    AdvancedParametersProvider* m_defaultAdvancedParametersProviders = nullptr;
    std::map<QString, AdvancedParametersProvider*> m_advancedParametersProvidersByParameterId;
};

} // namespace resource
} // namespace mediaserver
} // namespace nx
