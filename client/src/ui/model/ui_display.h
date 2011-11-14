#ifndef QN_UI_DISPLAY_H
#define QN_UI_DISPLAY_H

#include <QObject>
#include <core/resource/resource_consumer.h>

class QnMediaResource;
class QnAbstractArchiveReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class CLCamDisplay;

class QnUiDisplay: public QObject, protected QnResourceConsumer {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param resource                  Resource that this item is associated with. Must not be NULL.
     * \param parent                    Parent of this object.                
     */
    QnUiDisplay(const QnResourcePtr &resource, QObject *parent = NULL);

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
     * \returns                         Camera display associated with this item, if any.
     */
    CLCamDisplay *camDisplay() const {
        return m_camDisplay.data();
    }

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

protected:
    virtual void beforeDisconnectFromResource() override;

    virtual void disconnectFromResource() override;

private:
    /** Data provider for the associated resource. */
    QnAbstractStreamDataProvider *m_dataProvider;

    /** Media data provider, if any. */
    QnAbstractMediaStreamDataProvider *m_mediaProvider;

    /** Archive data provider, if any. */
    QnAbstractArchiveReader *m_archiveProvider;

    /** Camera display, if any. */
    QScopedPointer<CLCamDisplay> m_camDisplay;
};

#endif // QN_UI_DISPLAY_H
