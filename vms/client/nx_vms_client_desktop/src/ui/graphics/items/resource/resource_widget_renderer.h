// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QVector>

#include <client/client_globals.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/stoppable.h>
#include <ui/fisheye/fisheye_ptz_controller.h>
#include <utils/color_space/image_correction.h>
#include <utils/media/frame_info.h>

class QOpenGLWidget;
class QQuickWidget;
class QPainter;
class DecodedPictureToOpenGLUploader;
class QnGLRenderer;

/**
 * This class is supposed to be used from two threads &mdash; a <i>rendering</i> thread
 * and a <i>decoding</i> thread.
 *
 * Decoding thread prepares the frames to be displayed, and rendering thread displays them.
 *
 * Note that it is owned by the rendering thread.
 */

class QnResourceWidgetRenderer: public QObject, public QnStoppable
{
    Q_OBJECT

public:
    /**
     * @param context must not be null.
     */
    QnResourceWidgetRenderer(QObject* parent, QWidget* viewport);
    ~QnResourceWidgetRenderer();

    void setChannelCount(int channelCount);

    void pleaseStop();

    void update();
    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * It notifies the derived class that a new frame is available for display.
     * It is up to derived class to supply this new frame to the <i>rendering</i> thread.
     *
     * Note this method is not thread-safe and must be called from decoder thread only.
     * Note renderer is not required to draw image immediately. It is allowed to add frame to some
     * internal render queue and draw when appropriate.
     *
     * \param image                     New video frame.
     */
    void draw(const CLConstVideoDecoderOutputPtr& image, const QSize& onScreenSize);

    /**
     * Ignore frames currently in render queue and wait for current frame displayed.
     */
    void discardAllFramesPostedToDisplay(int channel);

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * It waits until given channel of the current frame (supplied via <tt>draw</tt>) is rendered.
     *
     * \param channel                   Channel number.
     */
    void waitForFrameDisplayed(int channel);
    void waitForQueueLessThan(int channel, int maxSize);

    /**
     * Blocks until all frames passed to \a draw are surely displayed on screen.
     * Difference from \a waitForFrameDisplayed is that waitForFrameDisplayed may not wait for
     * frames displayed, but only ensure, they will be displayed sometimes. This is required to take
     * advantage of async frame uploading and for effective usage of hardware decoder: it should
     * spend as much time as possible in \a decode method, but not waiting for frame to be rendered.
     */
    void finishPostedFramesRender(int channel);

    /**
     * This function may be called from any thread.
     * It is called just before this object is destroyed.
     */
    void destroyAsync();
    void inUse();
    void notInUse();

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     *
     * \param channel                   Channel number.
     * \returns                         Size of the given channel on rendering device.
     */
    QSize sizeOnScreen(int channel) const;

    /**
     * This function is supposed to be called from <i>decoding</t> thread.
     *
     * \returns                         Whether the downscale factor is forced x2 constant.
     */
    bool constantDownscaleFactor() const;

    void setChannelScreenSize(const QSize& screenSize);

    /** Set blur in range [0..1]. */
    void setBlurFactor(qreal value);

    Qn::RenderStatus paint(
        QPainter* painter, int channel, const QRectF& sourceRect,
        const QRectF& targetRect, qreal opacity);
    Qn::RenderStatus discardFrame(int channel);
    void skip(int channel);

    /**
     * Returns timestamp of frame that will be rendered next. It can be already displayed frame
     * (if no new frames available)
     */
    std::chrono::microseconds getTimestampOfNextFrameToRender(int channel) const;
    void blockTimeValue(int channel, std::chrono::microseconds timestamp);
    void unblockTimeValue(int channel);
    bool isTimeBlocked(int channel) const;

    bool isLowQualityImage(int channel) const;
    bool isHardwareDecoderUsed(int channel) const;

    std::chrono::microseconds lastDisplayedTimestamp(int channel) const;

    QSize sourceSize() const;

    QOpenGLWidget* openGLWidget() const;

    void setImageCorrection(const nx::vms::api::ImageCorrectionData& value);
    void setFisheyeController(QnFisheyePtzController* controller);

    void setDisplayedRect(int channel, const QRectF& rect);

    bool isEnabled(int channel) const;

    /*!
        Enable/disable frame rendering for channel \a channelNumber. true by default.
        Disabling causes all previously posted frames being discarded. Any subsequent frames will be ignored
    */
    void setEnabled(int channel, bool enabled);

    /*!
        Inform render that media stream is paused and no more frames expected
    */
    void setPaused(bool value);
    void setScreenshotInterface(ScreenshotInterface* value);
    void setHistogramConsumer(QnHistogramConsumer* value);


signals:
    void canBeDestroyed();
    /** This signal is emitted whenever the source geometry is changed. */
    void sourceSizeChanged();

    void beforeDestroy();
    void fisheyeCenterChanged(QPointF center, qreal radius);

private:
    QSize getMostFrequentChannelSourceSize() const;

private:
    struct RenderingContext
    {
        std::shared_ptr<QnGLRenderer> renderer;
        std::shared_ptr<DecodedPictureToOpenGLUploader> uploader;
        bool timestampBlocked = false;
        std::chrono::microseconds forcedTimestampValue{AV_NOPTS_VALUE};
        int framesSinceJump = 0;
        bool renderingEnabled = true;
        QSize channelSourceSize;
        QRectF displayRect{0, 0, 1, 1};
    };

    /** Renderering contexts that are used to render the channels. */
    QVector<RenderingContext> m_renderingContexts;

    /** Current source size, in square pixels. */
    QSize m_sourceSize;

    /** Mutex that is used for synchronization. */
    mutable nx::Mutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;

    QOpenGLWidget* const m_openGLWidget;
    QQuickWidget* m_quickWidget = nullptr;

    ScreenshotInterface* m_screenshotInterface = nullptr;
    int m_panoFactor = 1;
    qreal m_blurFactor = 0;

private:
    int m_useCount = 0;
    bool m_needStop = false;
    nx::Mutex m_usingMutex;
};
