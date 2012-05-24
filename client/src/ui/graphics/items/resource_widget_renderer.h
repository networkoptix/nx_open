#ifndef QN_DISPLAY_WIDGET_RENDERER_H
#define QN_DISPLAY_WIDGET_RENDERER_H

#include <QObject>
#include <QMutex>
#include <camera/abstractrenderer.h>
#include <camera/render_status.h>

class QThread;

class QnGLRenderer;

class QnResourceWidgetRenderer: public QObject, public QnAbstractRenderer {
    Q_OBJECT;
public:
    QnResourceWidgetRenderer(int channelCount, QObject *parent = NULL, const QGLContext *context = NULL);

    void update();

    virtual ~QnResourceWidgetRenderer();

	virtual void draw(CLVideoDecoderOutput *image) override;

    virtual void waitForFrameDisplayed(int channel) override;

	virtual void beforeDestroy() override;

	virtual QSize sizeOnScreen(unsigned int channel) const override;

    virtual bool constantDownscaleFactor() const override;

    void setChannelScreenSize(const QSize &screenSize);

    Qn::RenderStatus paint(int channel, const QRectF &rect, qreal opacity);

    qint64 lastDisplayedTime(int channel) const;

    qint64 isLowQualityImage(int channel) const;

    QnMetaDataV1Ptr lastFrameMetadata(int channel) const;

    QSize sourceSize() const;

signals:
    /**
     * This signal is emitted whenever the source geometry is changed.
     * 
     * \param newSourceSize             New source size.
     */
    void sourceSizeChanged(const QSize &newSourceSize);

private:
    /** Renderers that are used to render the channels. */
    QList<QnGLRenderer *> m_channelRenderers;

    /** Current source size, in square pixels. */
    QSize m_sourceSize;

    /** Mutex that is used for synchronization. */
    mutable QMutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;
};

#endif // QN_DISPLAY_WIDGET_RENDERER_H
