#include "text_pixmap_cache.h"

#include <QtCore/QHash>
#include <QtGui/QFont>

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
    QnTextPixmapCachePrivate() {
        return;
    }

    virtual ~QnTextPixmapCachePrivate() {
        qDeleteAll(pixmapByKey);
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

    QHash<Key, const QPixmap *> pixmapByKey;
};


QnTextPixmapCache::QnTextPixmapCache(QObject *parent): 
    QObject(parent),
    d(new QnTextPixmapCachePrivate())
{

}

QnTextPixmapCache::~QnTextPixmapCache() {
    return;
}

Q_GLOBAL_STATIC(QnTextPixmapCache, qn_textPixmapCache_instance)

QnTextPixmapCache *QnTextPixmapCache::instance() {
    return qn_textPixmapCache_instance();
}

const QPixmap *QnTextPixmapCache::pixmap(const QString &text, const QFont &font, const QColor &color) {
    QColor localColor = color.toRgb();
    if(!localColor.isValid())
        localColor = QColor(0, 0, 0, 255);

    QnTextPixmapCachePrivate::Key key(text, font, localColor);

    QHash<QnTextPixmapCachePrivate::Key, const QPixmap *>::const_iterator pos = d->pixmapByKey.find(key);
    if(pos != d->pixmapByKey.end())
        return *pos;

    QPixmap *result = new QPixmap(renderText(
        text,
        QPen(localColor, 0), 
        font
    ));

    return d->pixmapByKey[key] = result;
}




