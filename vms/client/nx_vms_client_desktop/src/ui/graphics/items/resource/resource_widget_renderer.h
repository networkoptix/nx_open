#pragma once

//#define TEST_FISHEYE_CALIBRATOR

#include <QtCore/QObject>
#include <QtCore/QVector>

#include <nx/utils/thread/mutex.h>

#include <client/client_globals.h>

#include <camera/abstract_renderer.h>
#include "utils/color_space/image_correction.h"
#include "core/resource/resource_media_layout.h"
#include "ui/fisheye/fisheye_ptz_controller.h"

class QGLContext;
class DecodedPictureToOpenGLUploader;
class QnGLRenderer;

class QnResourceWidgetRenderer: public QnAbstractRenderer
{
    Q_OBJECT

public:
    /**
     * @param context must not be null.
     */
    QnResourceWidgetRenderer(QObject* parent, QGLContext* context);
    virtual ~QnResourceWidgetRenderer() override;

    void setChannelCount(int channelCount);

    virtual void pleaseStop() override;

    void update();
    /**
     * Note this method is not thread-safe and must be called from decoder thread only.
     * Note renderer is not required to draw image immediately. It is allowed to add frame to some
     * internal render queue and draw when appropriate.
     */
    virtual void draw(const QSharedPointer<CLVideoDecoderOutput>& image) override;

    /** Ignore frames currently in render queue. */
    virtual void discardAllFramesPostedToDisplay(int channel) override;

    /** Blocks till last frame passed to draw method is displayed on screen. */
    virtual void waitForFrameDisplayed(int channel) override;
    virtual void waitForQueueLessThan(int channel, int maxSize) override;

    virtual void finishPostedFramesRender(int channel) override;

    virtual void destroyAsync() override;

    virtual QSize sizeOnScreen(int channel) const override;

    virtual bool constantDownscaleFactor() const override;

    void setChannelScreenSize(const QSize &screenSize);

    /** Set blur in range [0..1]. */
    void setBlurFactor(qreal value);

    Qn::RenderStatus paint(int channel, const QRectF &sourceRect, const QRectF &targetRect, qreal opacity);
    Qn::RenderStatus discardFrame(int channel);
    void skip(int channel); // TODO: #Elric replace with setEnabled

    virtual std::chrono::microseconds getTimestampOfNextFrameToRender(int channel) const override;
    virtual void blockTimeValue(
        int channelNumber, std::chrono::microseconds timestamp) const override;
    virtual void unblockTimeValue(int channelNumber) const  override;
    virtual bool isTimeBlocked(int channelNumber) const override;


    bool isLowQualityImage(int channel) const;
    bool isHardwareDecoderUsed(int channel) const;

    std::chrono::microseconds lastDisplayedTimestamp(int channel) const;

    QSize sourceSize() const;

    const QGLContext* glContext() const;

    bool isDisplaying(const QSharedPointer<CLVideoDecoderOutput>& image) const override;

    void setImageCorrection(const ImageCorrectionParams& value);
    void setFisheyeController(QnFisheyePtzController* controller);

    void setDisplayedRect(int channel, const QRectF& rect);

    virtual bool isEnabled(int channelNumber) const override;
    virtual void setEnabled(int channel, bool enabled) override;

    virtual void setPaused(bool value) override;
    virtual void setScreenshotInterface(ScreenshotInterface* value) override;
    void setHistogramConsumer(QnHistogramConsumer* value);

signals:
    /** This signal is emitted whenever the source geometry is changed. */
    void sourceSizeChanged();

    void beforeDestroy();
    void fisheyeCenterChanged(QPointF center, qreal radius);

private:
    QSize getMostFrequentChannelSourceSize() const;

private:
    struct RenderingTools
    {
        QnGLRenderer* renderer = nullptr;
        DecodedPictureToOpenGLUploader* uploader = nullptr;
        bool timestampBlocked = false;
        std::chrono::microseconds forcedTimestampValue{AV_NOPTS_VALUE};
        int framesSinceJump = 0;
    };

    /** Renderers that are used to render the channels. */
    mutable QVector<RenderingTools> m_channelRenderers;

    /** Current source size, in square pixels. */
    QSize m_sourceSize;

    QVector<QSize> m_channelSourceSize;

    /** Mutex that is used for synchronization. */
    mutable QnMutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;

    QGLContext* m_glContext;

    QRectF m_displayRect[CL_MAX_CHANNELS];

    QVector<bool> m_renderingEnabled;
    ScreenshotInterface* m_screenshotInterface = nullptr;
    int m_panoFactor = 1;
    qreal m_blurFactor = 0;
#ifdef TEST_FISHEYE_CALIBRATOR
    bool m_isCircleDetected;
#endif
};
