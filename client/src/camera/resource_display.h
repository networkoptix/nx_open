#ifndef QN_UI_DISPLAY_H
#define QN_UI_DISPLAY_H

#include <QObject>
#include <core/resource/resource_consumer.h>
#include <core/resource/media_resource.h>

class QnAbstractArchiveReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnCamDisplay;
class QnLongRunnable;
class QnAbstractRenderer;
class QnVideoCamera;

namespace detail {
    class QnRendererGuard: public QObject {
        Q_OBJECT;
    public:
        QnRendererGuard(QnAbstractRenderer *renderer): 
            m_renderer(renderer) 
        {}

        virtual ~QnRendererGuard();

        QnAbstractRenderer *renderer() const {
            return m_renderer;
        }

    private:
        QnAbstractRenderer *m_renderer;
    };

} // namespace detail


class QnResourceDisplay: public QObject, protected QnResourceConsumer {
    Q_OBJECT
public:
    /**
     * Constructor.
     * 
     * \param resource                  Resource that this display is associated with. Must not be NULL.
     * \param parent                    Parent of this display.                
     */
    QnResourceDisplay(const QnResourcePtr &resource, QObject *parent);

    /**
     * Virtual destructor. 
     */
    virtual ~QnResourceDisplay();

    /**
     * \returns                         Resource associated with this display.
     */
    const QnResourcePtr &resource() const {
        return getResource();
    }

    /**
     * \returns                         Media resource associated with this display, if any.
     */
    const QnMediaResourcePtr &mediaResource() const {
        return m_mediaResource;
    }

    /**
     * \returns                         Data provider associated with this display.
     */
    QnAbstractStreamDataProvider *dataProvider() const {
        return m_dataProvider;
    }

    /**
     * \returns                         Media data provider associated with this display, if any.
     */
    QnAbstractMediaStreamDataProvider *mediaProvider() const {
        return m_mediaProvider;
    }

    /**
     * \returns                         Archive reader associated with this display, if any.
     */
    QnAbstractArchiveReader *archiveReader() const {
        return m_archiveReader;
    }

    /**
     * \returns                         Video camera associated with this display, if any.
     */
    QnVideoCamera *camera() const {
        return m_camera;
    }

    /**
     * \returns                         Camdisplay for this display, if any.
     */
    QnCamDisplay *camDisplay() const;

    /**
     * \returns                         Video resource layout, if any, 
     */
    const QnResourceVideoLayout *videoLayout() const;

    /**
     * \returns                         Length of this display, in microseconds. If the length is not defined, returns -1.
     */
    qint64 lengthUSec() const;

    /**
     * \returns                         Current time of this display, in microseconds. If the time is not defined, returns -1.
     */
    qint64 currentTimeUSec() const;

    /**
     * \param usec                      New current time for this display, in microseconds.
     */
    void setCurrentTimeUSec(qint64 usec) const;

    /**
     * \returns                         Whether this display is paused. 
     */
    bool isPaused();

    void start();

    void play();

    void pause();

    bool isStillImage() const;

    /**
     * \param renderer                  Renderer to assign to this display. Ownership of the renderer is transferred to this display. 
     */
    void addRenderer(QnAbstractRenderer *renderer);

protected:
    virtual void beforeDisconnectFromResource() override;

    virtual void disconnectFromResource() override;

private:
    void cleanUp(QnLongRunnable *runnable) const;

private:
    /** Media resource. */
    QnMediaResourcePtr m_mediaResource;

    /** Data provider for the associated resource. */
    QnAbstractStreamDataProvider *m_dataProvider;

    /** Media data provider. */
    QnAbstractMediaStreamDataProvider *m_mediaProvider;

    /** Archive data provider. */
    QnAbstractArchiveReader *m_archiveReader;

    /** Video camera. */
    QnVideoCamera *m_camera; // TODO: Compatibility layer. Remove.

    /** Whether this display was started. */
    bool m_started;

    /** List of associated renderer guards. */
    QList<detail::QnRendererGuard *> m_guards;
};

#endif // QN_UI_DISPLAY_H
