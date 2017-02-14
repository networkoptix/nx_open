#pragma once

#include <QtGui/QCursor>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

class QnCursorCachePrivate;

class QnCursorCache: public QObject
{
    Q_OBJECT

public:
    QnCursorCache(QObject* parent = nullptr);
    virtual ~QnCursorCache();

    static QnCursorCache* instance();

    QCursor cursor(Qt::CursorShape shape, qreal rotation);
    QCursor cursor(Qt::CursorShape shape, qreal rotation, qreal precision);
    QCursor cursorForWindowSection(
        Qt::WindowFrameSection section, qreal rotation);
    QCursor cursorForWindowSection(
        Qt::WindowFrameSection section, qreal rotation, qreal precision);

private:
    QScopedPointer<QnCursorCachePrivate> d;
};
