#ifndef QN_SIZE_MOVE_WORKAROUND_H
#define QN_SIZE_MOVE_WORKAROUND_H

#include <QtCore/QObject>
#include <QtCore/QAbstractNativeEventFilter>

/**
 * Workaround for some Qt5-related bugs.
 * 
 * Bugs:
 * https://bugreports.qt-project.org/browse/QTBUG-28513
 * https://bugreports.qt-project.org/browse/QTBUG-32835 (Fixed in #Qt5.2.0) 
 */
class QnQt5Workaround: public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    QnQt5Workaround(QObject *parent = NULL);
    virtual ~QnQt5Workaround();

    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

private:
    bool m_inSizeMove;
    bool m_ignoreNextPostedEventsMessage;
};


#endif // QN_SIZE_MOVE_WORKAROUND_H
