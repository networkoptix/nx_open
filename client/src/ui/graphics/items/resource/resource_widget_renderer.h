#ifndef QN_DISPLAY_WIDGET_RENDERER_H
#define QN_DISPLAY_WIDGET_RENDERER_H

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <client/client_globals.h>

#include <camera/abstract_renderer.h>
#include "utils/color_space/image_correction.h"
#include "core/resource/resource_media_layout.h"
#include "ui/fisheye/fisheye_ptz_controller.h"

class QThread;
class QGLContext;
class DecodedPictureToOpenGLUploader;
class QnGLRenderer;

class QnResourceWidgetRenderer
:
    public QnAbstractRenderer
{
    Q_OBJECT;

public:
    /*!
        \param context MUST not be NULL
    */
    QnResourceWidgetRenderer(QObject* parent, const QGLContext* context );
    void setChannelCount(int channelCount);


    virtual ~QnResourceWidgetRenderer();

    //!Implementation of QnStoppable::pleaseStop()
    virtual void pleaseStop() override;

    void update();
    /*!
        \note This method is not thread-safe and must be called from decoder thread only
        \note Renderer is not required to draw \a image immediately. It is allowed to add frame to some internal render queue and draw when appropriate
    */
    virtual void draw( const QSharedPointer<CLVideoDecoderOutput>& image ) override;
    //!Ignore frames currently in render queue
    virtual void discardAllFramesPostedToDisplay(int channel) override;
    //!Blocks till last frame passed to \a draw method is displayed on screen
    virtual void waitForFrameDisplayed(int channel) override;
    //!Implementation of finishPostedFramesRender QnAbstractRenderer::finishPostedFramesRender
    virtual void finishPostedFramesRender(int channel) override;

    virtual void destroyAsync() override;

    virtual QSize sizeOnScreen(unsigned int channel) const override;

    virtual bool constantDownscaleFactor() const override;

    void setChannelScreenSize(const QSize &screenSize);

    Qn::RenderStatus paint(int channel, const QRectF &sourceRect, const QRectF &targetRect, qreal opacity);
    void skip(int channel); // TODO: #Elric replace with setEnabled

    virtual qint64 getTimestampOfNextFrameToRender(int channel) const override;
    virtual void blockTimeValue(int channelNumber, qint64  timestamp ) const  override;
    virtual void unblockTimeValue(int channelNumber) const  override;
    virtual bool isTimeBlocked(int channelNumber) const override;


    qint64 isLowQualityImage(int channel) const;
    bool isHardwareDecoderUsed(int channel) const;

    QnMetaDataV1Ptr lastFrameMetadata(int channel) const;

    QSize sourceSize() const;

    const QGLContext* glContext() const;

    bool isDisplaying( const QSharedPointer<CLVideoDecoderOutput>& image ) const;

    void setImageCorrection(const ImageCorrectionParams& value);
    void setFisheyeController(QnFisheyePtzController* controller);

    void setDisplayedRect(int channel, const QRectF& rect);

    //!Implementation of QnAbstractRenderer::isEnabled
    virtual bool isEnabled(int channelNumber) const override;
    //!Implementation of QnAbstractRenderer::setEnabled
    virtual void setEnabled(int channelNumber, bool enabled) override;

    virtual void setPaused(bool value) override;
    virtual void setScreenshotInterface(ScreenshotInterface* value) override;
    void setHystogramConsumer(QnHistogramConsumer* value);

signals:
    /**
     * This signal is emitted whenever the source geometry is changed.
     * 
     * \param newSourceSize             New source size.
     */
    void sourceSizeChanged();
    void beforeDestroy();
private:
    struct RenderingTools
    {
        QnGLRenderer* renderer;
        DecodedPictureToOpenGLUploader* uploader;
        bool timestampBlocked;
        qint64 forcedTimestampValue;
        int framesSinceJump;

        RenderingTools()
        :
            renderer( NULL ),
            uploader( NULL ),
            timestampBlocked( false ),
            forcedTimestampValue( AV_NOPTS_VALUE ),
            framesSinceJump( 0 )
        {
        }
    };

    /** Renderers that are used to render the channels. */
    mutable std::vector<RenderingTools> m_channelRenderers;

    /** Current source size, in square pixels. */
    QSize m_sourceSize;

    /** Mutex that is used for synchronization. */
    mutable QMutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;

    const QGLContext* m_glContext;
    
    QRectF m_displayRect[CL_MAX_CHANNELS];

    std::vector<bool> m_renderingEnabled;
    ScreenshotInterface* m_screenshotInterface;
};

#endif // QN_DISPLAY_WIDGET_RENDERER_H
