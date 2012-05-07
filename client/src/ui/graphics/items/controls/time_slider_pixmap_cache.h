#ifndef QN_TIME_SLIDER_PIXMAP_CACHE_H
#define QN_TIME_SLIDER_PIXMAP_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <QtGui/QPixmap>
#include <QtGui/QFont>

#include <utils/common/hash.h>
#include "time_step.h"

/**
 * Global pixmap cache for time slider.
 * 
 * Note that OpenGL paint engine texture caching gets messed up when pixmaps 
 * are copied around. This is why it is important not to copy the pixmaps 
 * returned from the cache and pass them as-is to the painter.
 * 
 * To make accidental copying less probable, getter functions return 
 * pointers instead of references.
 */
class QnTimeSliderPixmapCache: public QObject {
    Q_OBJECT;
public:
    QnTimeSliderPixmapCache(QObject *parent = NULL);
    virtual ~QnTimeSliderPixmapCache();

    static QnTimeSliderPixmapCache *instance();

    const QPixmap *positionShortPixmap(qint64 position, int height, const QnTimeStep &step);
    const QPixmap *positionLongPixmap(qint64 position, int height, const QnTimeStep &step);
    const QPixmap *textPixmap(const QString &text, int height, const QColor &color);

private:
    struct TextCacheKey {
        TextCacheKey(QString text, int size, QColor color): text(text), size(size), color(color) {}
        TextCacheKey(): size(0) {}

        QString text;
        int size;
        QColor color;

        friend uint qHash(const TextCacheKey &key) {
            using ::qHash;

            uint h1 = qHash(key.text);
            uint h2 = qHash(key.size);
            uint h3 = qHash(key.color);
            
            return h1 ^ ((h2 << 16) | (h2 >> 16)) ^ h3;
        }

        friend bool operator==(const TextCacheKey &l, const TextCacheKey &r) {
            return l.text == r.text && l.size == r.size && l.color == r.color;
        }
    };

    QHash<qint32, const QPixmap *> m_pixmapByShortPositionKey;
    QHash<QnTimeStepLongCacheKey, const QPixmap *> m_pixmapByLongPositionKey;
    QHash<TextCacheKey, const QPixmap *> m_pixmapByCacheKey;
    QFont m_font;
};

#endif // QN_TIME_SLIDER_PIXMAP_CACHE_H
