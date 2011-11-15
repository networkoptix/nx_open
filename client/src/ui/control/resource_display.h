#ifndef QN_UI_DISPLAY_H
#define QN_UI_DISPLAY_H

#include <QObject>
#include <core/resource/resource_consumer.h>

class QnMediaResource;
class QnAbstractArchiveReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class QnVideoResourceLayout;
class CLCamDisplay;
class CLLongRunnable;
class CLAbstractRenderer;

namespace detail {
    class QnRendererGuard: public QObject {
        Q_OBJECT;
    public:
        QnRendererGuard(CLAbstractRenderer *renderer): 
            m_renderer(renderer) 
        {}

        ~QnRendererGuard();

    private:
        CLAbstractRenderer *m_renderer;
    };
} // namespace detail

class QnResourceDisplay: public QObject, protected QnResourceConsumer {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param resource                  Resource that this item is associated with. Must not be NULL.
     * \param parent                    Parent of this object.                
     */
    QnResourceDisplay(const QnResourcePtr &resource, QObject *parent = NULL);

    /**
     * Virtual destructor. 
     */
    virtual ~QnResourceDisplay();

    /**
     * \returns                         Resource associated with this item.
     */
    QnResource *resource() const;

    /**
     * \returns                         Media resource associated with this item, if any.
     */
    QnMediaResource *mediaResource() const;

    /**
     * \returns                         Data provider associated with this item.
     */
    QnAbstractStreamDataProvider *dataProvider() const {
        return m_dataProvider;
    }

    /**
     * \returns                         Media data provider associated with this item, if any.
     */
    QnAbstractMediaStreamDataProvider *mediaProvider() const {
        return m_mediaProvider;
    }

    /**
     * \returns                         Video resource layout, if any, 
     */
    const QnVideoResourceLayout *videoLayout() const;

    /**
     * \returns                         Length of this item, in microseconds. If the length is not defined, returns -1.
     */
    qint64 lengthUSec() const;

    /**
     * \returns                         Current time of this item, in microseconds. If the time is not defined, returns -1.
     */
    qint64 currentTimeUSec() const;

    /**
     * \param usec                      New current time for this item, in microseconds.
     */
    void setCurrentTimeUSec(qint64 usec) const;

    void start();

    void play();

    void pause();

    /**
     * \param renderer                  Renderer to assign to this display. Ownership of the renderer is transferred to this display. 
     */
    void addRenderer(CLAbstractRenderer *renderer);

protected:
    virtual void beforeDisconnectFromResource() override;

    virtual void disconnectFromResource() override;

private:
    void cleanUp(CLLongRunnable *runnable) const;

private:
    /** Media resource, if any. */
    QnMediaResource *m_mediaResource;

    /** Data provider for the associated resource. */
    QnAbstractStreamDataProvider *m_dataProvider;

    /** Media data provider, if any. */
    QnAbstractMediaStreamDataProvider *m_mediaProvider;

    /** Archive data provider, if any. */
    QnAbstractArchiveReader *m_archiveProvider;

    /** Camera display, if any. */
    CLCamDisplay *m_camDisplay;

    /** Whether this display was started. */
    bool m_started;

    /** List of associated renderer guards. */
    QList<detail::QnRendererGuard *> m_guards;
};

#endif // QN_UI_DISPLAY_H
