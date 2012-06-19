#include "text_pixmap_cache.h"

#include <QtCore/QCache>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>

#include <utils/common/hashed_font.h>
#include <utils/common/hash.h>

namespace {
    QPixmap renderText(const QString &text, const QPen &pen, const QFont &font) {
        QFontMetrics metrics(font);
        QSize textSize = metrics.size(Qt::TextSingleLine, text);
        if(textSize.isEmpty())
            return QPixmap();

        QPixmap pixmap(textSize.width(), metrics.height());
        pixmap.fill(QColor(0, 0, 0, 0));

        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter | Qt::TextSingleLine, text);
        painter.end();

        return pixmap;
    }

} // anonymous namespace

class QnTextPixmapCachePrivate {
public:
    QnTextPixmapCachePrivate(): 
        pixmapByKey(64 * 1024 * 1024) /* 64 Megabytes. */ 
    {}

    virtual ~QnTextPixmapCachePrivate() {
        return;
    }

    struct Key {
        QString text;
        QnHashedFont font;
        QColor color;

        Key(const QString &text, const QFont &font, const QColor &color): text(text), font(font), color(color) {}

        friend uint qHash(const Key &key) {
            using ::qHash;

            return 
                (929 * qHash(key.text)) ^
                (883 * qHash(key.font)) ^
                (547 * qHash(key.color));
        }

        bool operator==(const Key &other) const {
            return text == other.text && font == other.font && color == other.color;
        }
    };

    QCache<Key, const QPixmap> pixmapByKey;
};


QnTextPixmapCache::QnTextPixmapCache(QObject *parent): 
    QObject(parent),
    d(new QnTextPixmapCachePrivate())
{}

QnTextPixmapCache::~QnTextPixmapCache() {
    return;
}

Q_GLOBAL_STATIC(QnTextPixmapCache, qn_textPixmapCache_instance)

QnTextPixmapCache *QnTextPixmapCache::instance() {
    return qn_textPixmapCache_instance();
}

const QPixmap &QnTextPixmapCache::pixmap(const QString &text, const QFont &font, const QColor &color) {
    QColor localColor = color.toRgb();
    if(!localColor.isValid())
        localColor = QColor(0, 0, 0, 255);

    QnTextPixmapCachePrivate::Key key(text, font, localColor);
    const QPixmap *result = d->pixmapByKey.object(key);
    if(result)
        return *result;

    result = new QPixmap(renderText(
        text,
        QPen(localColor, 0), 
        font
    ));
    d->pixmapByKey.insert(key, result, result->height() * result->width() * result->depth() / 8);

    return *result;
}




