#ifndef QN_CURSOR_CACHE_H
#define QN_CURSOR_CACHE_H

#include <QtGui/QCursor>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

class QnCursorCachePrivate;

class QnCursorCache: public QObject {
    Q_OBJECT;
public:
    QnCursorCache(QObject *parent = NULL);
    virtual ~QnCursorCache();

    static QnCursorCache *instance();

    QCursor cursor(Qt::CursorShape shape, qreal rotation);
    QCursor cursor(Qt::CursorShape shape, qreal rotation, qreal precision);

private:
    QScopedPointer<QnCursorCachePrivate> d;
};

#endif // QN_CURSOR_CACHE_H
