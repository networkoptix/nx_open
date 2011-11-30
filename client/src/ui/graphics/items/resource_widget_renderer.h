#ifndef QN_DISPLAY_WIDGET_RENDERER_H
#define QN_DISPLAY_WIDGET_RENDERER_H

#include <QObject>
#include <QMutex>
#include <camera/abstractrenderer.h>
#include <camera/render_status.h>

class QThread;

class CLGLRenderer;

class QnResourceWidgetRenderer: public QObject, public CLAbstractRenderer, public QnRenderStatus {
    Q_OBJECT;
public:
    QnResourceWidgetRenderer(int channelCount, QObject *parent = NULL);

    virtual ~QnResourceWidgetRenderer();

	virtual void draw(CLVideoDecoderOutput *image) override;

    virtual void waitForFrameDisplayed(int channel) override;

	virtual void beforeDestroy() override;

	virtual QSize sizeOnScreen(unsigned int channel) const override;

    virtual bool constantDownscaleFactor() const override;

    void setChannelScreenSize(const QSize &screenSize);

    RenderStatus paint(int channel, const QRectF &rect);

    qint64 lastDisplayedTime(int channel) const;
signals:
    /**
     * This signal is emitted whenever the source geometry is changed.
     * 
     * \param newSourceSize             New source size.
     */
    void sourceSizeChanged(const QSize &newSourceSize);

private:
    void checkThread(bool inDecodingThread) const;

private:
    /** Renderers that are used to render the channels. */
    QList<CLGLRenderer *> m_channelRenderers;

    /** Current source size, in square pixels. */
    QSize m_sourceSize;

    /** Display thread that this channelRenderer is associated with. Used only in debug for error checking. */
    mutable QThread *m_decodingThread;

    /** Mutex that is used for synchronization. */
    mutable QMutex m_mutex;

    /** Current screen size of a single channel, in pixels. */
    QSize m_channelScreenSize;
};

#endif // QN_DISPLAY_WIDGET_RENDERER_H
