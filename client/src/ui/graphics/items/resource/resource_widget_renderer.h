#ifndef QN_DISPLAY_WIDGET_RENDERER_H
#define QN_DISPLAY_WIDGET_RENDERER_H

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <camera/abstract_renderer.h>
#include <camera/render_status.h>


class QThread;
class QGLContext;
class DecodedPictureToOpenGLUploader;
class QnGLRenderer;

class QnResourceWidgetRenderer
:
    public QObject,
    public QnAbstractRenderer
{
    Q_OBJECT;

public:
    /*!
        \param context MUST not be NULL
    */
    QnResourceWidgetRenderer( int channelCount, QObject* parent, const QGLContext* context );

    virtual ~QnResourceWidgetRenderer();

    void update();
    /*!
        \note This method is not thread-safe and must be called from decoder thread only
    */
    virtual void draw( const QSharedPointer<CLVideoDecoderOutput>& image ) override;

    virtual void waitForFrameDisplayed(int channel) override;

    virtual void beforeDestroy() override;

    virtual QSize sizeOnScreen(unsigned int channel) const override;

    virtual bool constantDownscaleFactor() const override;

    void setChannelScreenSize(const QSize &screenSize);

    Qn::RenderStatus paint(int channel, const QRectF &rect, qreal opacity);

    virtual qint64 lastDisplayedTime(int channel) const override;

    qint64 isLowQualityImage(int channel) const;

    QnMetaDataV1Ptr lastFrameMetadata(int channel) const;

    QSize sourceSize() const;

    const QGLContext* glContext() const;

signals:
    /**
     * This signal is emitted whenever the source geometry is changed.
     * 
     * \param newSourceSize             New source size.
     */
    void sourceSizeChanged(const QSize &newSourceSize);

private:
    struct RenderingTools
    {
        QnGLRenderer* renderer;
        DecodedPictureToOpenGLUploader* uploader;

        RenderingTools()
        :
            renderer( NULL ),
            uploader( NULL )
        {
        }
    };

    /** Renderers that are used to render the channels. */
    std::vector<RenderingTools> m_channelRenderers;

    /** Current source size, in square pixels. */
    QSize m_sourceSize;

    /** Mutex that is used for synchronization. */
    mutable QMutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;

    const QGLContext* m_glContext;
};

#endif // QN_DISPLAY_WIDGET_RENDERER_H
