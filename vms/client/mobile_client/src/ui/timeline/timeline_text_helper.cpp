#include "timeline_text_helper.h"

#include <algorithm>

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>

#include <nx/utils/log/log.h>

namespace
{
    const int kMinTextureSize = 256;
    const int kMaxTextureSize = 8192;

    class StringPacker
    {
        struct StringInfo
        {
            QString string;
            int width;

            bool operator <(const StringInfo& other) const
            {
                return width > other.width;
            }
        };

        struct Line
        {
            QStringList strings;
            int width = 0;
        };

        QStringList result;

        QList<StringInfo> measuredStrings;
        int lineHeight;
        int textureSize;

    public:
        StringPacker(const QStringList& strings, const QFontMetrics& fm)
            : result(strings)
            , lineHeight(fm.height())
            , textureSize(kMinTextureSize)
        {
            for (const auto& string: strings)
            {
                const auto info = StringInfo{ string, fm.size(Qt::TextSingleLine, string).width() };
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

class QnTimelineTextHelperPrivate
{
public:
    QImage texture;
    QFont font;
    int lineHeight;
    int digitWidth;
    QHash<QString, QRect> strings;
    int maxCharWidth;
    int spaceWidth;
    qreal pixelRatio = qApp->devicePixelRatio();

    QnTimelineTextHelperPrivate(const QFont& font)
        : font(font)
    {
        this->font.setPixelSize(font.pixelSize() * pixelRatio);
        const auto fm = QFontMetrics(this->font);
        lineHeight = fm.height();
        digitWidth = fm.size(Qt::TextSingleLine, "0").width();
        maxCharWidth = fm.size(Qt::TextSingleLine, "m").width();
        spaceWidth = fm.size(Qt::TextSingleLine, " ").width();
    }

    void addString(const QString& string, QPainter* painter, int& x, int& y)
    {
        if (strings.contains(string))
            return;

        const auto fm = QFontMetrics(this->font);
        const auto size = fm.size(Qt::TextSingleLine, string);
        if (x > 0 && x + size.width() >= texture.width())
        {
            x = 0;
            y += lineHeight;
        }

        painter->drawText(x, y + fm.ascent(), string);
        strings.insert(string, QRect(x, y, size.width(), size.height()));
        x += size.width();
        if (x > texture.width() || y + lineHeight > texture.height())
            NX_WARNING(this, "String is out of bounds: \"%1\"", string);
    }
};

QnTimelineTextHelper::QnTimelineTextHelper(
        const QFont& font,
        const QColor& color,
        const QStringList& strings)
    : d_ptr(new QnTimelineTextHelperPrivate(font))
{
    Q_D(QnTimelineTextHelper);

    auto packedStrings = strings;
    for (int i = 0; i <= 9; ++i)
        packedStrings.append(QString::number(i));

    auto stringPacker = StringPacker(packedStrings, QFontMetrics(d->font));

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
        for (const auto& string: packedStrings)
            d->addString(string, &painter, x, y);
    }
}

QnTimelineTextHelper::~QnTimelineTextHelper()
{
}

QRectF QnTimelineTextHelper::digitCoordinates(int digit) const
{
    return stringCoordinates(QString::number(digit));
}

QRectF QnTimelineTextHelper::stringCoordinates(const QString& string) const
{
    Q_D(const QnTimelineTextHelper);

    const auto rect = QRectF(d->strings.value(string));
    if (rect.isNull())
        return QRectF();

    const auto w = d->texture.width();
    const auto h = d->texture.height();
    return QRectF(rect.x() / w, rect.y() / h, rect.width() / w, rect.height() / h);
}

QSize QnTimelineTextHelper::stringSize(const QString& string) const
{
    Q_D(const QnTimelineTextHelper);
    return d->strings.value(string).size() / d->pixelRatio;
}

QSize QnTimelineTextHelper::digitSize() const
{
    Q_D(const QnTimelineTextHelper);
    return QSize(d->digitWidth, d->lineHeight) / d->pixelRatio;
}

QImage QnTimelineTextHelper::texture() const
{
    Q_D(const QnTimelineTextHelper);
    return d->texture;
}

int QnTimelineTextHelper::maxCharWidth() const
{
    Q_D(const QnTimelineTextHelper);
    return d->maxCharWidth / d->pixelRatio;
}

int QnTimelineTextHelper::lineHeight() const
{
    Q_D(const QnTimelineTextHelper);
    return d->lineHeight / d->pixelRatio;
}

int QnTimelineTextHelper::spaceWidth() const
{
    Q_D(const QnTimelineTextHelper);
    return d->spaceWidth / d->pixelRatio;
}

void StringPacker::pack()
{
    textureSize = kMinTextureSize;
    while (textureSize <= kMaxTextureSize)
    {
        if (tryPack(textureSize))
            break;

        textureSize *= 2;
    }
}

bool StringPacker::tryPack(int size)
{
    QList<Line> lines;

    int height = 0;

    for (const auto& info: measuredStrings)
    {
        if (info.width > size)
            return false;

        int i = 0;
        for (i = 0; i < lines.size(); ++i)
        {
            if (lines[i].width + info.width < size)
                break;
        }
        if (i == lines.size())
        {
            height += lineHeight;
            lines.append(Line());
        }

        if (height > size)
            return false;

        auto& line = lines[i];
        line.strings.append(info.string);
        line.width += info.width;
    }

    result.clear();
    for (const auto& line: lines)
        result.append(line.strings);

    return true;
}
