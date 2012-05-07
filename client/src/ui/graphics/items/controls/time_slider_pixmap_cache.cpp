#include "time_slider_pixmap_cache.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QFont>

#include <ui/common/image_processing.h>

#include "time_step.h"

namespace {
    QPixmap renderText(const QString &text, const QPen &pen, const QFont &font, int fontPixelSize = -1) {
        QFont actualFont = font;
        if(fontPixelSize != -1)
            actualFont.setPixelSize(fontPixelSize);

        QFontMetrics metrics(actualFont);
        QSize textSize = metrics.size(Qt::TextSingleLine, text);

        QPixmap pixmap(textSize.width(), metrics.height());
        pixmap.fill(QColor(0, 0, 0, 0));

        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setPen(pen);
        painter.setFont(actualFont);
        painter.drawText(pixmap.rect(), Qt::AlignCenter | Qt::TextSingleLine, text);
        painter.end();

        return pixmap;
    }

} // anonymous namespace

QnTimeSliderPixmapCache::QnTimeSliderPixmapCache(QObject *parent):
    QObject(parent)
{
    m_font = QApplication::font();
}

QnTimeSliderPixmapCache::~QnTimeSliderPixmapCache() {
    qDeleteAll(m_pixmapByCacheKey);
    m_pixmapByCacheKey.clear();
    m_pixmapByHighlightKey.clear();
    m_pixmapByHighlightKey.clear();
}

Q_GLOBAL_STATIC(QnTimeSliderPixmapCache, qn_timeSliderPixmapCacheInstance)

QnTimeSliderPixmapCache *QnTimeSliderPixmapCache::instance() {
    return qn_timeSliderPixmapCacheInstance();
}

const QPixmap *QnTimeSliderPixmapCache::positionPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = cacheKey(position, height, step);

    QHash<qint32, const QPixmap *>::const_iterator itr = m_pixmapByPositionKey.find(key);
    if(itr != m_pixmapByPositionKey.end())
        return *itr;

    return m_pixmapByPositionKey[key] = textPixmap(toString(position, step), height, QColor(255, 255, 255, 255));
}

const QPixmap *QnTimeSliderPixmapCache::highlightPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = cacheKey(position, height, step);

    QHash<qint32, const QPixmap *>::const_iterator itr = m_pixmapByPositionKey.find(key);
    if(itr != m_pixmapByPositionKey.end())
        return *itr;

    QString text = toLongString(position, step);
    QPixmap basePixmap = *textPixmap(text, height, QColor(0, 0, 0, 255));
    QPixmap overPixmap = *textPixmap(text, height, QColor(255, 255, 255, 255));

    QImage image(basePixmap.size() + QSize(6, 3), QImage::Format_ARGB32);
    image.fill(qRgba(0, 0, 0, 0));
    {
        QPainter painter(&image);
        painter.drawPixmap(3, 0, basePixmap);
    }
    image = gaussianBlur(image, 1.5);
    QPixmap blurPixmap = QPixmap::fromImage(image);
    {
        QPainter painter(&image);
        for(int i = 0; i < 8; i++)
            painter.drawPixmap(0, 0, blurPixmap);
        painter.drawPixmap(3, 0, overPixmap);
    }

    return m_pixmapByPositionKey[key] = new QPixmap(QPixmap::fromImage(image));
}

const QPixmap *QnTimeSliderPixmapCache::textPixmap(const QString &text, int height, const QColor &color) {
    QColor localColor = color.toRgb();
    if(!localColor.isValid())
        localColor = QColor(0, 0, 0, 255);

    CacheKey key(text, height, localColor);

    QHash<CacheKey, const QPixmap *>::const_iterator itr = m_pixmapByCacheKey.find(key);
    if(itr != m_pixmapByCacheKey.end())
        return *itr;

    QPixmap *result = new QPixmap(renderText(
        text,
        QPen(localColor, 0), 
        m_font, 
        height
    ));

    return m_pixmapByCacheKey[key] = result;
}

