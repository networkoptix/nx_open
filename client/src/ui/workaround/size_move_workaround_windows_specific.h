#ifndef QN_SIZE_MOVE_WORKAROUND_H
#define QN_SIZE_MOVE_WORKAROUND_H

#include <QtCore/QObject>
#include <QtCore/QAbstractNativeEventFilter>

/**
 * Workaround for a strange Qt bug that causes system freeze when resizing any 
 * window.
 * 
 * Related Qt bug:
 * https://bugreports.qt-project.org/browse/QTBUG-28513
 */
class QnSizeMoveWorkaround: public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    QnSizeMoveWorkaround(QObject *parent = NULL);
    virtual ~QnSizeMoveWorkaround();

    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

private:
    bool m_inSizeMove;
    bool m_ignoreNextPostedEventsMessage;
};


#endif // QN_SIZE_MOVE_WORKAROUND_H
