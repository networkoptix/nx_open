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
 * \note This class is <b>not</b> thread-safe.
 */
class QnTextPixmapCache: public QObject {
    Q_OBJECT;
public:
    QnTextPixmapCache(QObject *parent = NULL);
    virtual ~QnTextPixmapCache();

    static QnTextPixmapCache *instance();

    const QPixmap &pixmap(const QString &text, const QFont &font, const QColor &color);

private:
    QScopedPointer<QnTextPixmapCachePrivate> d;
};


#endif // QN_TEXT_PIXMAP_CACHE_H
