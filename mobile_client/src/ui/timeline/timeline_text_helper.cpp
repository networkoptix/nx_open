#include "timeline_text_helper.h"

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtCore/QDebug>

namespace {
    const int charsPerLine = 12;
}

class QnTimelineTextHelperPrivate {
public:
    QImage texture;
    QFontMetrics fm;
    int lineHeight;
    int digitWidth;
    QHash<QString, QRect> strings;
    int maxCharWidth;
    int spaceWidth;

    QnTimelineTextHelperPrivate(const QFont &font) :
        fm(font),
        lineHeight(fm.height()),
        digitWidth(fm.size(Qt::TextSingleLine, lit("0")).width()),
        maxCharWidth(fm.size(Qt::TextSingleLine, lit("m")).width()),
        spaceWidth(fm.size(Qt::TextSingleLine, lit(" ")).width())
    {}

    void drawNumbers(QPainter *painter) {
        int y = fm.ascent();
        for (int i = 0; i <= 9; ++i) {
            if (digitWidth * (i + 1) > texture.width())
                qWarning() << "QnTimelineTextHelper: Digit is out of bounds:" << i;
            painter->drawText(digitWidth * i, y, QString::number(i));
        }
    }

    void addString(const QString &string, QPainter *painter, int &x, int &y) {
        if (strings.contains(string))
            return;

        QSize size = fm.size(Qt::TextSingleLine, string);
        if (x > 0 && x + size.width() >= texture.width()) {
            x = 0;
            y += lineHeight;
        }

        painter->drawText(x, y + fm.ascent(), string);
        strings.insert(string, QRect(x, y, size.width(), size.height()));
        x += size.width();
        if (x > texture.width() || y + lineHeight > texture.height())
            qWarning() << "QnTimelineTextHelper: String is out of bounds:" << string;
    }
};

QnTimelineTextHelper::QnTimelineTextHelper(const QFont &font, const QColor &color, const QStringList &strings) :
    d(new QnTimelineTextHelperPrivate(font))
{
    int textureSize = 128;
    int desiredSize = charsPerLine * maxCharWidth();
    while (textureSize < desiredSize)
        textureSize *= 2;

    d->texture = QImage(textureSize, textureSize, QImage::Format_RGBA8888);
    d->texture.fill(Qt::transparent);

    {
        QPainter painter(&d->texture);
        painter.setFont(font);
        painter.setPen(color);
        painter.setBrush(color);

        d->drawNumbers(&painter);

        int x = d->digitWidth * 10;
        int y = 0;
        for (const QString &string: strings)
            d->addString(string, &painter, x, y);
    }
}

QnTimelineTextHelper::~QnTimelineTextHelper() {
}

QRectF QnTimelineTextHelper::digitCoordinates(int digit) const {
    if (digit > 9 || digit < 0)
        return QRectF();

    qreal w = d->texture.width();
    qreal h = d->texture.height();

    return QRectF(digit * d->digitWidth / w, 0.0, d->digitWidth / w, d->lineHeight / h);
}

QRectF QnTimelineTextHelper::stringCoordinates(const QString &string) const {
    QRect rect = d->strings.value(string);
    if (rect.isNull())
        return QRectF();

    qreal w = d->texture.width();
    qreal h = d->texture.height();
    return QRectF(rect.x() / w, rect.y() / h, rect.width() / w, rect.height() / h);
}

QSize QnTimelineTextHelper::stringSize(const QString &string) const {
    return d->strings.value(string).size();
}

QSize QnTimelineTextHelper::digitSize() const {
    return QSize(d->digitWidth, d->lineHeight);
}

QImage QnTimelineTextHelper::texture() const {
    return d->texture;
}

int QnTimelineTextHelper::maxCharWidth() const {
    return d->maxCharWidth;
}

int QnTimelineTextHelper::lineHeight() const {
    return d->lineHeight;
}

int QnTimelineTextHelper::spaceWidth() const {
    return d->spaceWidth;
}
