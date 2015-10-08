#include "bookmark_item.h"

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>

#include <core/resource/camera_bookmark.h>

namespace {
    const int maximumBookmarkWidth = 250;
    const int padding = 8;
    const int spacing = 8;
    const int maximumCaptionLength = 64;
    const int maximumDescriptionLength = 256;
    const int radius = 4;

    QString elidedString(const QString &string, int length) {
        static const QString tail = lit("...");

        if (string.length() <= length + tail.length())
            return string;

        return string.left(length) + tail;
    }
}

class QnBookmarkItemPrivate {
    Q_DECLARE_PUBLIC(QnBookmarkItem)
    QnBookmarkItem *q_ptr;

public:
    QnBookmarkItemPrivate(QnBookmarkItem *parent);

    QnCameraBookmark bookmark;
    QPixmap pixmap;
    QnBookmarkColors colors;

    void updatePixmap();
};

QnBookmarkItem::QnBookmarkItem(QGraphicsItem *parent)
    : base_type(parent)
    , d_ptr(new QnBookmarkItemPrivate(this))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QnBookmarkItem::QnBookmarkItem(const QnCameraBookmark &bookmark, QGraphicsItem *parent)
    : base_type(parent)
    , d_ptr(new QnBookmarkItemPrivate(this))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setBookmark(bookmark);
}

QnBookmarkItem::~QnBookmarkItem() {
}

const QnCameraBookmark &QnBookmarkItem::bookmark() const {
    Q_D(const QnBookmarkItem);
    return d->bookmark;
}

void QnBookmarkItem::setBookmark(const QnCameraBookmark &bookmark) {
    Q_D(QnBookmarkItem);
    bool updatePixmap = d->bookmark.name != bookmark.name ||
                      d->bookmark.description != bookmark.description;

    d->bookmark = bookmark;

    if (updatePixmap)
        d->updatePixmap();
}

void QnBookmarkItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget);

    Q_D(QnBookmarkItem);

    if (!d->pixmap.isNull())
        painter->drawPixmap(0, 0, d->pixmap);
}

const QnBookmarkColors &QnBookmarkItem::colors() const {
    Q_D(const QnBookmarkItem);
    return d->colors;
}

void QnBookmarkItem::setColors(const QnBookmarkColors &colors) {
    Q_D(QnBookmarkItem);

    d->colors = colors;
    update();
}


QnBookmarkItemPrivate::QnBookmarkItemPrivate(QnBookmarkItem *parent)
    : q_ptr(parent)
{
}

void QnBookmarkItemPrivate::updatePixmap() {
    Q_Q(QnBookmarkItem);

    QString caption = elidedString(bookmark.name, maximumCaptionLength);
    QString description = elidedString(bookmark.description, maximumDescriptionLength);
    int maxTextWidth = maximumBookmarkWidth - 2 * padding;

    QFont descriptionFont = q->font();
    QFont captionFont = q->font();
    captionFont.setBold(true);

    QRect captionRect;
    QRect descriptionRect;

    int y = padding;
    if (!caption.isEmpty()) {
        QFontMetrics fm(captionFont);

        captionRect = fm.boundingRect(padding, y, maxTextWidth, INT_MAX, Qt::TextWordWrap, caption);
        y = captionRect.bottom();
    }
    if (!description.isEmpty()) {
        QFontMetrics fm(descriptionFont);

        if (!caption.isEmpty())
            y += spacing;

        descriptionRect = fm.boundingRect(padding, y, maxTextWidth, INT_MAX, Qt::TextWordWrap, description);
        y = descriptionRect.bottom();
    }
    y += padding;

    pixmap = QPixmap(maximumBookmarkWidth, y);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    painter.setBrush(colors.background);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(pixmap.rect(), radius, radius);
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.setPen(colors.text);

    painter.setFont(captionFont);
    painter.drawText(captionRect, Qt::TextWordWrap, caption);

    painter.setFont(descriptionFont);
    painter.drawText(descriptionRect, Qt::TextWordWrap, description);

    q->setMinimumSize(QSizeF(pixmap.size()));
    q->setMaximumSize(QSizeF(pixmap.size()));
    q->update();
}
