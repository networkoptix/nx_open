#pragma once

#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

class QnQtbugWorkaroundPrivate;

/**
 * Workaround for some Qt bugs.
 *
 * Windows bugs:
 * https://bugreports.qt-project.org/browse/QTBUG-28513
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
