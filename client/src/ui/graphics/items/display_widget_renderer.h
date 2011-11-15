#ifndef QN_DISPLAY_WIDGET_RENDERER_H
#define QN_DISPLAY_WIDGET_RENDERER_H

#include <QMutex>
#include <camera/abstractrenderer.h>

class QThread;

class CLGLRenderer;
class QnResourceDisplay;

class QnDisplayWidgetRenderer: public QObject, public CLAbstractRenderer {
    Q_OBJECT;
public:
    QnDisplayWidgetRenderer(int channelCount, QObject *parent = NULL);

    virtual ~QnDisplayWidgetRenderer();

	virtual void draw(CLVideoDecoderOutput *image) override;

    virtual void waitForFrameDisplayed(int channel) override;

	virtual void beforeDestroy() override;

	virtual QSize sizeOnScreen(unsigned int channel) const override;

    virtual bool constantDownscaleFactor() const override;

    void setChannelScreenSize(const QSize &screenSize);

    bool paint(int channel, const QRectF &rect);

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
