#ifndef QN_TEXT_PIXMAP_CACHE_H
#define QN_TEXT_PIXMAP_CACHE_H

#include <QtCore/QObject>

class QPixmap;
class QFont;
class QColor;

class QnTextPixmapCachePrivate;

/**
 * Global pixmap cache for rendered text pieces.
 * 
 * Note that OpenGL paint engine texture caching gets messed up when pixmaps 
 * are copied around. This is why it is important not to copy the pixmaps 
 * returned from the cache and pass them as-is to the painter.
 * 
 * To make accidental copying less probable, getter functions return 
 * pointers instead of references.
 */
class QnTextPixmapCache: public QObject {
    Q_OBJECT;
public:
    QnTextPixmapCache(QObject *parent = NULL);
    virtual ~QnTextPixmapCache();

    static QnTextPixmapCache *instance();

    const QPixmap *pixmap(const QString &text, const QFont &font, const QColor &color);

private:
    QScopedPointer<QnTextPixmapCachePrivate> d;
};


#endif // QN_TEXT_PIXMAP_CACHE_H
