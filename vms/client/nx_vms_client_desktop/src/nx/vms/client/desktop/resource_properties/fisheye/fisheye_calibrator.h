// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QImage>

#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/thread/long_runnable.h>

namespace nx::vms::client::desktop {

/**
 * This class analyzes image data to compute fisheye ellipse center and radii.
 */
class NX_VMS_CLIENT_DESKTOP_API FisheyeCalibrator: public QnLongRunnable
{
    Q_OBJECT
    Q_PROPERTY(QPointF center READ center NOTIFY centerChanged)
    Q_PROPERTY(qreal radius READ radius NOTIFY radiusChanged)
    Q_PROPERTY(qreal stretch READ stretch NOTIFY stretchChanged)
    Q_PROPERTY(qreal running READ isRunning NOTIFY runningChanged)

public:
    FisheyeCalibrator();
    virtual ~FisheyeCalibrator() override;

    enum class Result
    {
        ok,
        errorNotFisheyeImage,
        errorTooLowLight,
        errorInterrupted, //< Process was interrupted before getting any result.
        errorInvalidInput //< Input image is invalid.
    };
    Q_ENUM(Result)

    /**
     * Analyze souce image and compute fisheye parameters.
     */
    Q_INVOKABLE void analyzeFrameAsync(QImage frame);

    /**
     * Computed values.
     */
    QPointF center() const;
    qreal stretch() const;
    qreal radius() const;

    /**
     * Set initial approximation.
     */
    void setCenter(const QPointF& value);
    void setStretch(const qreal& value);
    void setRadius(qreal value);

signals:
    void centerChanged(const QPointF& center);
    void radiusChanged(qreal radius);
    void stretchChanged(qreal stretch);
    void finished(nx::vms::client::desktop::FisheyeCalibrator::Result result);
    void runningChanged();

protected:
    Result analyzeFrame(QImage frame);

    virtual void run() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

NX_VMS_CLIENT_DESKTOP_API std::ostream& operator<<(
    std::ostream& os, FisheyeCalibrator::Result value);

} // namespace nx::vms::client::desktop
