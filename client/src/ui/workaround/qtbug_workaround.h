#ifndef QN_QTBUG_WORKAROUND_H
#define QN_QTBUG_WORKAROUND_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractNativeEventFilter>

class QnQtbugWorkaroundPrivate;

/**
 * Workaround for some Qt-related bugs.
 * 
 * Windows bugs:
 * https://bugreports.qt-project.org/browse/QTBUG-28513
 * https://bugreports.qt-project.org/browse/QTBUG-32835 (Fixed in #Qt5.2.0) 
 * https://bugreports.qt-project.org/browse/QTBUG-806
 */
class QnQtbugWorkaround: public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    QnQtbugWorkaround(QObject *parent = NULL);
    virtual ~QnQtbugWorkaround();

    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

protected:
    Q_DECLARE_PRIVATE(QnQtbugWorkaround)

private:
    QScopedPointer<QnQtbugWorkaroundPrivate> d_ptr;
};


namespace QnEvent {
    /** Windows-specific event type for system menu, activated via Alt-Space. */
    static const QEvent::Type WinSystemMenu = static_cast<QEvent::Type>(QEvent::User + 0x5481);
}


#endif // QN_QTBUG_WORKAROUND_H
