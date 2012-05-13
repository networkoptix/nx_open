#include "time_slider_pixmap_cache.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QFont>

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
    m_pixmapByShortPositionKey.clear();
    m_pixmapByLongPositionKey.clear();
}

Q_GLOBAL_STATIC(QnTimeSliderPixmapCache, qn_timeSliderPixmapCacheInstance)

QnTimeSliderPixmapCache *QnTimeSliderPixmapCache::instance() {
    return qn_timeSliderPixmapCacheInstance();
}

const QPixmap *QnTimeSliderPixmapCache::positionShortPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = shortCacheKey(position, height, step);

    QHash<qint32, const QPixmap *>::const_iterator itr = m_pixmapByShortPositionKey.find(key);
    if(itr != m_pixmapByShortPositionKey.end())
        return *itr;

    return m_pixmapByShortPositionKey[key] = textPixmap(toShortString(position, step), height, QColor(255, 255, 255, 255));
}

const QPixmap *QnTimeSliderPixmapCache::positionLongPixmap(qint64 position, int height, const QnTimeStep &step) {
    QnTimeStepLongCacheKey key = longCacheKey(position, height, step);

    QHash<QnTimeStepLongCacheKey, const QPixmap *>::const_iterator itr = m_pixmapByLongPositionKey.find(key);
    if(itr != m_pixmapByLongPositionKey.end())
        return *itr;

    return m_pixmapByLongPositionKey[key] = textPixmap(toLongString(position, step), height, QColor(255, 255, 255, 255));
}

const QPixmap *QnTimeSliderPixmapCache::textPixmap(const QString &text, int height, const QColor &color) {
    QColor localColor = color.toRgb();
    if(!localColor.isValid())
        localColor = QColor(0, 0, 0, 255);

    TextCacheKey key(text, height, localColor);

    QHash<TextCacheKey, const QPixmap *>::const_iterator itr = m_pixmapByCacheKey.find(key);
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

