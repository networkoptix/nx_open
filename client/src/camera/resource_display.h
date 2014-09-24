#ifndef QN_UI_DISPLAY_H
#define QN_UI_DISPLAY_H

#include <QtCore/QObject>
#include <core/resource/resource_consumer.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

class QnAbstractArchiveReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnCamDisplay;
class QnLongRunnable;
class QnAbstractRenderer;
class QnClientVideoCamera;
class QnCounter;

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
     * Called while corresponding QGLWidget and QGLContext are still alive to OGL resources to be properly removed
     */
    void beforeDestroy();

    /**
     * \returns                         Resource associated with this display.
     */
    const QnResourcePtr &resource() const;

    /**
     * \returns                         Media resource associated with this display, if any.
     */
    const QnMediaResourcePtr &mediaResource() const;

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
    QnClientVideoCamera *camera() const {
        return m_camera;
    }

    /**
     * \returns                         Camdisplay for this display, if any.
     */
    QnCamDisplay *camDisplay() const;

    /**
     * \returns                         Video resource layout, if any, 
     */
    QnConstResourceVideoLayoutPtr videoLayout() const;

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
    void removeRenderer(QnAbstractRenderer *renderer);

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
    QnClientVideoCamera *m_camera; // TODO: #Elric Compatibility layer. Remove.

    /** Whether this display was started. */
    bool m_started;

    QnCounter *m_counter;
};

typedef QSharedPointer<QnResourceDisplay> QnResourceDisplayPtr;

#endif // QN_UI_DISPLAY_H
