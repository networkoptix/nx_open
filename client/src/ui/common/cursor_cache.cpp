#include "cursor_cache.h"

#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <utils/common/warnings.h>
#include <utils/math/math.h>

#include <platform/platform_abstraction.h>

#include <ui/common/geometry.h>


// -------------------------------------------------------------------------- //
// QnCursorCachePrivate
// -------------------------------------------------------------------------- //
class QnCursorCachePrivate: protected QnGeometry {
public:
    QCursor cursor(Qt::CursorShape shape, int rotation) {
        int key = (shape << 16) + rotation;
        if(m_cache.contains(key))
            return m_cache.value(key);

        QCursor result;
        if(renderCursor(shape, rotation, &result)) {
            m_cache.insert(key, result);
            return result;
        } else {
            m_cache.insert(key, shape);
            return shape;
        }
    }

private:
    bool renderCursor(Qt::CursorShape shape, int rotation, QCursor *result) {
        if(!qnPlatform) {
            qnWarning("Platform instance does not exist, cannot render cursor.");
            return false;
        }

        QCursor cursor = qnPlatform->images()->bitmapCursor(shape);
        if(cursor.shape() != Qt::BitmapCursor)
            return false; /* Not supported by the platform images implementation. */

        QPixmap normalPixmap = cursor.pixmap();
        
        QPixmap rotatedPixmap(normalPixmap.size() * 1.5);
        rotatedPixmap.fill(Qt::transparent);
        
        QTransform transform;
        transform.translate(rotatedPixmap.width() / 2, rotatedPixmap.height() / 2);
        transform.rotate(rotation);
        transform.translate(-normalPixmap.width() / 2, -normalPixmap.height() / 2);

        QPainter painter(&rotatedPixmap);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setTransform(transform);
        painter.drawPixmap(QPoint(0, 0), normalPixmap);
        painter.end();

        QPoint hotSpot = transform.map(cursor.hotSpot());

        *result = QCursor(rotatedPixmap, hotSpot.x(), hotSpot.y());
        return true;
    }

private:
    QHash<int, QCursor> m_cache;
};


// -------------------------------------------------------------------------- //
// QnCursorCache
// -------------------------------------------------------------------------- //
QnCursorCache::QnCursorCache(QObject *parent):
    QObject(parent),
    d(new QnCursorCachePrivate())
{}

QnCursorCache::~QnCursorCache() {
    return;
}

Q_GLOBAL_STATIC(QnCursorCache, qn_cursorCache_instance)

QnCursorCache *QnCursorCache::instance() {
    return qn_cursorCache_instance();
}

QCursor QnCursorCache::cursor(Qt::CursorShape shape, qreal rotation) {
    return d->cursor(shape, static_cast<int>(qMod(rotation, 360.0)));
}

QCursor QnCursorCache::cursor(Qt::CursorShape shape, qreal rotation, qreal precision) {
    return d->cursor(shape, static_cast<int>(qRound(qMod(rotation, 360.0), precision)));
}

