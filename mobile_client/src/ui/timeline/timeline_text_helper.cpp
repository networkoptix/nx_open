#include "timeline_text_helper.h"

#include <algorithm>

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtCore/QDebug>

namespace {
    const int minTextureSize = 256;
    const int maxTextureSize = 8192;

    class StringPacker {
        struct StringInfo {
            QString string;
            int width;

            StringInfo(const QString &string, int width) : string(string), width(width) {}

            bool operator <(const StringInfo &other) const {
                return width > other.width;
            }
        };

        struct Line {
            QStringList strings;
            int width;

            Line() : width(0) {}
        };

        QStringList result;

        QList<StringInfo> measuredStrings;
        int lineHeight;
        int textureSize;

    public:
        StringPacker(const QStringList &strings, const QFontMetrics &fm)
            : result(strings)
            , lineHeight(fm.height())
            , textureSize(minTextureSize)
        {
            for (const QString &string: strings) {
                StringInfo info(string, fm.size(Qt::TextSingleLine, string).width());
                measuredStrings.insert(std::lower_bound(measuredStrings.begin(), measuredStrings.end(), info), info);
            }

            pack();

            measuredStrings.clear();
        }

        int resultSize() const { return textureSize; }
        QStringList resultStrings() { return result; }

    private:
        void pack();
        bool tryPack(int resultSize);
    };

}

class QnTimelineTextHelperPrivate {
public:
    QImage texture;
    QFont font;
    QFontMetrics fm;
    int lineHeight;
    int digitWidth;
    QHash<QString, QRect> strings;
    int maxCharWidth;
    int spaceWidth;
    qreal pixelRatio;

    QnTimelineTextHelperPrivate(QFont font, qreal pixelRatio)
        : font((font.setPixelSize(font.pixelSize() * pixelRatio), font))
        , fm(this->font)
        , lineHeight(fm.height())
        , digitWidth(fm.size(Qt::TextSingleLine, lit("0")).width())
        , maxCharWidth(fm.size(Qt::TextSingleLine, lit("m")).width())
        , spaceWidth(fm.size(Qt::TextSingleLine, lit(" ")).width())
        , pixelRatio(pixelRatio)
    {}

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

QnTimelineTextHelper::QnTimelineTextHelper(const QFont &font, qreal pixelRatio, const QColor &color, const QStringList &strings) :
    d(new QnTimelineTextHelperPrivate(font, pixelRatio))
{
    QStringList packedStrings = strings;
    for (int i = 0; i <= 9; ++i)
        packedStrings.append(QString::number(i));

    StringPacker stringPacker(packedStrings, d->fm);

    int textureSize = stringPacker.resultSize();
    packedStrings = stringPacker.resultStrings();

    d->texture = QImage(textureSize, textureSize, QImage::Format_RGBA8888);
    d->texture.fill(Qt::transparent);

    {
        QPainter painter(&d->texture);
        painter.setFont(d->font);
        painter.setPen(color);
        painter.setBrush(color);

        int x = 0;
        int y = 0;
        for (const QString &string: packedStrings)
            d->addString(string, &painter, x, y);
    }
}

QnTimelineTextHelper::~QnTimelineTextHelper() {
}

QRectF QnTimelineTextHelper::digitCoordinates(int digit) const {
    return stringCoordinates(QString::number(digit));
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
    return d->strings.value(string).size() / pixelRatio();
}

QSize QnTimelineTextHelper::digitSize() const {
    return QSize(d->digitWidth, d->lineHeight) / pixelRatio();
}

QImage QnTimelineTextHelper::texture() const {
    return d->texture;
}

int QnTimelineTextHelper::maxCharWidth() const {
    return d->maxCharWidth / d->pixelRatio;
}

int QnTimelineTextHelper::lineHeight() const {
    return d->lineHeight / d->pixelRatio;
}

int QnTimelineTextHelper::spaceWidth() const {
    return d->spaceWidth / d->pixelRatio;
}

qreal QnTimelineTextHelper::pixelRatio() const {
    return d->pixelRatio;
}

void StringPacker::pack() {
    textureSize = minTextureSize;
    while (textureSize <= maxTextureSize) {
        if (tryPack(textureSize))
            break;

        textureSize *= 2;
    }
}

bool StringPacker::tryPack(int size) {
    QList<Line> lines;

    int height = 0;

    for (const StringInfo &info: measuredStrings) {
        if (info.width > size)
            return false;

        int i = 0;
        for (i = 0; i < lines.size(); ++i) {
            if (lines[i].width + info.width < size)
                break;
        }
        if (i == lines.size()) {
            height += lineHeight;
            lines.append(Line());
        }

        if (height > size)
            return false;

        Line &line = lines[i];
        line.strings.append(info.string);
        line.width += info.width;
    }

    result.clear();
    for (const Line &line: lines)
        result.append(line.strings);

    return true;
}
