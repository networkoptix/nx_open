#include "cursor_cache.h"

#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <utils/common/warnings.h>
#include <utils/math/math.h>

#include <platform/platform_abstraction.h>

#include <ui/common/frame_section.h>

class QnCursorCachePrivate
{
public:
    QCursor cursor(Qt::CursorShape shape, int rotation)
    {
        int key = (shape << 16) + rotation;
        if(m_cache.contains(key))
            return m_cache.value(key);

        QCursor result;
        if (renderCursor(shape, rotation, &result))
        {
            m_cache.insert(key, result);
            return result;
        }
        else
        {
            m_cache.insert(key, shape);
            return shape;
        }
    }

    QCursor cursorForWindowSection(Qt::WindowFrameSection section, int rotation)
    {
        auto shape = Qn::calculateHoverCursorShape(section);

        #if defined(Q_OS_LINUX)
            /* Linux cursor schemes can have cursors for each section corner
               plus separate fdiag and bdiag cursors.
               Qt support only diagonal cursors but loads TopRight and BottomRight cursors instead.
               It seems however xcb somehow knows that the app does not support corner icons
               and gives fdiag/bdiag cursors.
               Ideally these cursors should look like double-arrow but for unknown reason
               cursor scheme authors use two of the corner cursors instead.
               Indeed it seems they use two random cursors.
               In this situation the simplest way to make our resizing cursors look at least
               adequate is to take one cursor and rotate it to any angle.
               BDiag cursor is the best candidate because it points to the top right corner
               in most cases.
            */
            switch (section)
            {
                case Qt::TopLeftSection:
                    rotation = (rotation - 90) % 360;
                    shape = Qt::SizeBDiagCursor;
                    break;
                case Qt::BottomRightSection:
                    rotation = (rotation + 90) % 360;
                    shape = Qt::SizeBDiagCursor;
                    break;
                case Qt::BottomLeftSection:
                    rotation = (rotation + 180) % 360;
                    shape = Qt::SizeBDiagCursor;
                    break;
                default:
                    break;
            }
        #endif

        return cursor(shape, rotation);
    }

private:
    bool renderCursor(Qt::CursorShape shape, int rotation, QCursor* result)
    {
        if (!qnPlatform)
        {
            qnWarning("Platform instance does not exist, cannot render cursor.");
            return false;
        }

        const auto cursor = qnPlatform->images()->bitmapCursor(shape);
        if (cursor.shape() != Qt::BitmapCursor)
        {
            // Not supported by the platform images implementation.
            return false;
        }

        const auto normalPixmap = cursor.pixmap();

        QPixmap rotatedPixmap(normalPixmap.size() * 1.5);
        rotatedPixmap.fill(Qt::transparent);

        QTransform transform;

        transform.translate(rotatedPixmap.width() / 2, rotatedPixmap.height() / 2);
        transform.rotate(rotation);
        transform.translate(-normalPixmap.width() / 2, -normalPixmap.height() / 2);

        QPainter painter(&rotatedPixmap);

        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
        painter.setTransform(transform);
        painter.drawPixmap(QPoint(0, 0), normalPixmap);
        painter.end();

        const auto primaryScreenRatio = qApp->primaryScreen()->devicePixelRatio();
        rotatedPixmap.setDevicePixelRatio(primaryScreenRatio);
        const auto hotSpot = transform.map(cursor.hotSpot()) / primaryScreenRatio;
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

QnCursorCache::~QnCursorCache()
{
}

Q_GLOBAL_STATIC(QnCursorCache, qn_cursorCache_instance)

QnCursorCache* QnCursorCache::instance()
{
    return qn_cursorCache_instance();
}

QCursor QnCursorCache::cursor(Qt::CursorShape shape, qreal rotation)
{
    return d->cursor(shape, static_cast<int>(qMod(rotation, 360.0)));
}

QCursor QnCursorCache::cursor(Qt::CursorShape shape, qreal rotation, qreal precision)
{
    return d->cursor(shape, static_cast<int>(qRound(qMod(rotation, 360.0), precision)));
}

QCursor QnCursorCache::cursorForWindowSection(Qt::WindowFrameSection section, qreal rotation)
{
    return d->cursorForWindowSection(section, static_cast<int>(qMod(rotation, 360.0)));
}

QCursor QnCursorCache::cursorForWindowSection(
    Qt::WindowFrameSection section, qreal rotation, qreal precision)
{
    return d->cursorForWindowSection(section, static_cast<int>(qRound(qMod(rotation, 360.0), precision)));
}
