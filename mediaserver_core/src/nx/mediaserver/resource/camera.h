#pragma once

#include <core/resource/camera_resource.h>

namespace nx {
namespace mediaserver {
namespace resource {

class Camera: public QnVirtualCameraResource
{
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

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    int m_channelNumber; // video/audio source number
    QElapsedTimer m_lastInitTime;
    QAuthenticator m_lastCredentials;
};

} // namespace resource
} // namespace mediaserver
} // namespace nx
