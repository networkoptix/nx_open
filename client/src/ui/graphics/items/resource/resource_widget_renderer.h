#ifndef QN_DISPLAY_WIDGET_RENDERER_H
#define QN_DISPLAY_WIDGET_RENDERER_H

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <camera/abstract_renderer.h>
#include <camera/render_status.h>
#include "utils/color_space/image_correction.h"
#include "core/resource/resource_media_layout.h"


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
    void skip(int channel);

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

    void setDisplayedRect(int channel, const QRectF& rect);
signals:
    /**
     * This signal is emitted whenever the source geometry is changed.
     * 
     * \param newSourceSize             New source size.
     */
    void sourceSizeChanged();
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
};

#endif // QN_DISPLAY_WIDGET_RENDERER_H
