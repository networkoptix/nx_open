#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QVector>

#include <nx/utils/thread/mutex.h>

#include <client/client_globals.h>
#include <camera/abstract_renderer.h>
#include <utils/color_space/image_correction.h>
#include <ui/fisheye/fisheye_ptz_controller.h>

class QOpenGLWidget;
class DecodedPictureToOpenGLUploader;
class QnGLRenderer;

class QnResourceWidgetRenderer: public QnAbstractRenderer
{
    Q_OBJECT

public:
    /**
     * @param context must not be null.
     */
    QnResourceWidgetRenderer(QObject* parent, QOpenGLWidget* widget);
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

    void setChannelScreenSize(const QSize& screenSize);

    /** Set blur in range [0..1]. */
    void setBlurFactor(qreal value);

    Qn::RenderStatus paint(
        int channel, const QRectF& sourceRect, const QRectF& targetRect, qreal opacity);
    Qn::RenderStatus discardFrame(int channel);
    void skip(int channel); // TODO: #Elric replace with setEnabled

    virtual std::chrono::microseconds getTimestampOfNextFrameToRender(int channel) const override;
    virtual void blockTimeValue(int channel, std::chrono::microseconds timestamp) override;
    virtual void unblockTimeValue(int channel) override;
    virtual bool isTimeBlocked(int channel) const override;

    bool isLowQualityImage(int channel) const;
    bool isHardwareDecoderUsed(int channel) const;

    std::chrono::microseconds lastDisplayedTimestamp(int channel) const;

    QSize sourceSize() const;

    QOpenGLWidget* openGLWidget() const;

    bool isDisplaying(const QSharedPointer<CLVideoDecoderOutput>& image) const override;

    void setImageCorrection(const nx::vms::api::ImageCorrectionData& value);
    void setFisheyeController(QnFisheyePtzController* controller);

    void setDisplayedRect(int channel, const QRectF& rect);

    virtual bool isEnabled(int channel) const override;
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
    mutable QnMutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;

    QOpenGLWidget* m_openGLWidget;

    ScreenshotInterface* m_screenshotInterface = nullptr;
    int m_panoFactor = 1;
    qreal m_blurFactor = 0;
};
