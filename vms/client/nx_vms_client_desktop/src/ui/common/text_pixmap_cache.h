#ifndef QN_TEXT_PIXMAP_CACHE_H
#define QN_TEXT_PIXMAP_CACHE_H

#include <QtCore/QObject>

class QPixmap;
class QFont;
class QColor;

class QnTextPixmapCachePrivate;

struct QnTextPixmap;

/**
 * Global pixmap cache for rendered text pieces.
 *
 * \note This class is <b>not</b> thread-safe.
 */
class QnTextPixmapCache: public QObject
{
    Q_OBJECT

public:
    QnTextPixmapCache(QObject* parent = nullptr);
    virtual ~QnTextPixmapCache();

    static QnTextPixmapCache* instance();

    const QPixmap& pixmap(const QString& text, const QFont& font, const QColor& color);

    const QnTextPixmap& pixmap(const QString& text, const QFont& font, const QColor& color,
        int width, Qt::TextElideMode elideMode);

private:
    QScopedPointer<QnTextPixmapCachePrivate> d;
};

/*
Pixmap with additional information about text rendered into it
*/
struct QnTextPixmap
{
    const QPixmap pixmap;
    const QPoint origin;
    const QString renderedText;
    const Qt::TextElideMode elideMode;
    const int elidedForWidth;

    inline QSize size() const { return pixmap.size() / pixmap.devicePixelRatio(); }
    inline bool elided() const { return elidedForWidth > 0; }

    QnTextPixmap(): elideMode(Qt::ElideRight), elidedForWidth(0) {}

    QnTextPixmap(const QPixmap& pixmap, const QPoint& origin, const QString& renderedText,
                Qt::TextElideMode elideMode, int elidedForWidth):
        pixmap(pixmap), origin(origin), renderedText(renderedText),
        elideMode(elideMode), elidedForWidth(elidedForWidth)
    {
    }
};

#endif // QN_TEXT_PIXMAP_CACHE_H
