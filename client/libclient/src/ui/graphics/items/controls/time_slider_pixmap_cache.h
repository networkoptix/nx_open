#ifndef QN_TIME_SLIDER_PIXMAP_CACHE_H
#define QN_TIME_SLIDER_PIXMAP_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QCache>
#include <QtGui/QPixmap>
#include <QtGui/QFont>

#include "time_step.h"

class QnTextPixmapCache;

/**
 * Pixmap cache for time slider.
 *
 * \note This class is <b>not</t> thread-safe.
 * \see QnTextPixmapCache
 */
class QnTimeSliderPixmapCache: public QObject
{
    Q_OBJECT;
public:
    QnTimeSliderPixmapCache(int numLevels, QObject* parent = nullptr);
    virtual ~QnTimeSliderPixmapCache();

    int numLevels() const;

    const QFont& dateFont() const;
    void setDateFont(const QFont& font);

    const QColor& dateColor() const;
    void setDateColor(const QColor& color);

    const QFont& defaultFont() const;
    void setDefaultFont(const QFont& font);

    const QColor& defaultColor() const;
    void setDefaultColor(const QColor& color);

    const QFont& tickmarkFont(int level) const;
    void setTickmarkFont(int level, const QFont& font);

    const QColor& tickmarkColor(int level) const;
    void setTickmarkColor(int level, const QColor& color);

    const QPixmap& tickmarkTextPixmap(int level, qint64 position, int height, const QnTimeStep& step);
    const QPixmap& dateTextPixmap(qint64 position, int height, const QnTimeStep& step);

    const QPixmap& textPixmap(const QString& text, int height);
    const QPixmap& textPixmap(const QString& text, int height, const QColor& color);
    const QPixmap& textPixmap(const QString& text, int height, const QColor& color, const QFont& font);
    const QPixmap& textPixmap(const QString& text, const QColor& color, const QFont& font);

    Q_SLOT void clear();

private:
    QFont m_defaultFont;
    QColor m_defaultColor;

    QFont m_dateFont;
    QColor m_dateColor;

    QVector<QFont> m_tickmarkFonts;
    QVector<QColor> m_tickmarkColors;

    QnTextPixmapCache* m_cache;

    typedef QCache<qint32, const QPixmap> ShortKeyCache;
    QVector<ShortKeyCache*> m_pixmapByShortPositionKey;

    typedef QCache<QnTimeStepLongCacheKey, const QPixmap> LongKeyCache;
    LongKeyCache m_pixmapByLongPositionKey;
};

#endif // QN_TIME_SLIDER_PIXMAP_CACHE_H
