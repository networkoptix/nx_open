#include "text_image_filter.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QTextFrame>
#include <QtGui/QTextDocument>
#include <QtGui/QAbstractTextDocumentLayout>

#include <nx/streaming/config.h>

#include <utils/math/math.h>
#include <utils/media/frame_info.h>
#include <utils/color_space/yuvconvert.h>

namespace {

static constexpr int kMaxTextLinesInFrame = 20;
static constexpr int kMinTextHeight = 14;

using DocumentPtr = QSharedPointer<QTextDocument>;

Qt::Alignment getAlignFromCorner(Qt::Corner corner)
{
    return (corner == Qt::BottomLeftCorner || corner == Qt::TopLeftCorner)
        ? Qt::AlignLeft
        : Qt::AlignRight;
}

int getBufferYOffset(
    Qt::Corner corner,
    int frameHeight,
    int textHeight)
{
    return (corner == Qt::TopLeftCorner || corner == Qt::TopRightCorner)
        ? 0
        : frameHeight - textHeight;
}

QTextCharFormat getCharFormat()
{
    static const QColor kTextOutlineColor(32,32,32,80);

    QTextCharFormat result;
    result.setTextOutline(QPen(kTextOutlineColor));
    return result;
}

QTextBlockFormat getBlockFormat(const Qt::Alignment textAlignment)
{
    QTextBlockFormat result;
    result.setAlignment(textAlignment);
    return result;
}

void updateDocumentFont(
    const DocumentPtr& document,
    const QFont font,
    const Qt::Alignment textAlignment,
    const int offset)
{
    document->setDefaultFont(font);
    document->setDocumentMargin(QFontMetrics(font).averageCharWidth() / 2);
    const auto frame = document->rootFrame();
    auto format = frame->frameFormat();
    format.setTopMargin(0);
    format.setBottomMargin(0);

    if (offset != 0)
    {
        if (textAlignment == Qt::AlignLeft)
            format.setRightMargin(offset);
        else
            format.setLeftMargin(offset);
    }

    frame->setFrameFormat(format);
}

DocumentPtr makeDocument(
    const Qt::Alignment textAlignment,
    const QString& text,
    const int frameWidth,
    const int maxTextWidth,
    const int initialPixelSize)
{
    QFont font;
    font.setBold(true);
    font.setPixelSize(qMax(kMinTextHeight, initialPixelSize));

    const DocumentPtr document(new QTextDocument());
    const int offset = frameWidth - maxTextWidth;
    updateDocumentFont(document, font, textAlignment, offset);

    QTextCursor cursor(document.data());
    cursor.setBlockFormat(getBlockFormat(textAlignment));
    cursor.setBlockCharFormat(getCharFormat());
    cursor.insertText(text);

    // Looking for the most appropriate font size which fits all lines without word wrapping.
    while (font.pixelSize() > kMinTextHeight && document->idealWidth() > frameWidth)
    {
        font.setPixelSize(font.pixelSize() - 1);
        updateDocumentFont(document, font, textAlignment, offset);
    }

    document->setTextWidth(frameWidth);
    return document;
}

} // namespace

namespace nx::vms::common::transcoding {

struct TextImageFilter::Private
{
    TextGetter textGetter;
    Qt::Corner corner;

    /**
     * Optimization flag for single-channel cameras: do not
     * repaint text if the frame was not changed.
     */
    bool checkHash;
    qint64 hash = -1;

    QSize frameSize;
    QString text;
    DocumentPtr document;
    QSharedPointer<QImage> textImage;
    QSharedPointer<quint8> imageBuffer;

    int bufferYOffset = 0;
    qreal widthFactor = 1.0;

    Private(
        const VideoLayoutPtr& videoLayout,
        const Qt::Corner corner,
        const TextGetter& textGetter,
        const qreal widthFactor);

    void updateTextData(
        const QString& newText,
        const CLVideoDecoderOutputPtr& frame);

    qint64 calculateHash(
        const quint8* data,
        const QSize& size,
        int linesize);
};

TextImageFilter::Private::Private(
    const VideoLayoutPtr& videoLayout,
    const Qt::Corner corner,
    const TextGetter& textGetter,
    const qreal widthFactor)
    :
    textGetter(textGetter),
    corner(corner),
    checkHash(videoLayout && videoLayout->channelCount() > 1),
    widthFactor(widthFactor)
{
}

void TextImageFilter::Private::updateTextData(
    const QString& newText,
    const CLVideoDecoderOutputPtr& frame)
{
    text = newText;
    frameSize = frame ? frame->size() : QSize();
    if (!frame)
    {
        document.reset();
        return;
    }

    const int initialTextHeight = frameSize.height() / kMaxTextLinesInFrame;
    const auto align = getAlignFromCorner(corner);
    const int maxTextWidth = frameSize.width() * widthFactor;
    document = makeDocument(align, text, frameSize.width(), maxTextWidth, initialTextHeight);

    const QFontMetrics metrics(document->defaultFont());
    const unsigned int textWidth = document->textWidth();
    const int textHeight = document->size().height();

    bufferYOffset = qPower2Floor(getBufferYOffset(corner, frame->height, textHeight), 2);

    const auto drawWidth = qPower2Ceil(textWidth, CL_MEDIA_ALIGNMENT);
    const int bufferSize = drawWidth * textHeight * 4;
    imageBuffer.reset(
        static_cast<quint8*>(qMallocAligned(bufferSize, CL_MEDIA_ALIGNMENT)), &qFreeAligned);
    textImage.reset(new QImage(imageBuffer.data(), drawWidth, textHeight, drawWidth * 4,
        QImage::Format_ARGB32_Premultiplied));
}

qint64 TextImageFilter::Private::calculateHash(
    const quint8* data,
    const QSize& size,
    int linesize)
{
    qint64 result = 0;
    const qint64* data64 = reinterpret_cast<const qint64*>(data);
    for (int y = 0; y < size.height(); ++y)
    {
        for (int i = 0; i < static_cast<int>(size.width() / sizeof(qint64)); ++i)
            result ^= data64[i];

        data64 += linesize / sizeof(qint64);
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

QnAbstractImageFilterPtr TextImageFilter::create(
    const VideoLayoutPtr& videoLayout,
    const Qt::Corner corner,
    const TextGetter& linesGetter,
    const qreal widthFactor)
{
    return QnAbstractImageFilterPtr(
        new TextImageFilter(videoLayout, corner, linesGetter, widthFactor));
}

TextImageFilter::TextImageFilter(
    const VideoLayoutPtr& videoLayout,
    const Qt::Corner corner,
    const TextGetter& linesGetter,
    const qreal widthFactor)
    :
    d(new Private(videoLayout, corner, linesGetter, widthFactor))
{
}

TextImageFilter::~TextImageFilter()
{
}

CLVideoDecoderOutputPtr TextImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (!frame)
        return frame;

    const auto& text = d->textGetter(frame);
    const auto& frameSize = frame->size();
    if (text != d->text || frameSize != d->frameSize)
        d->updateTextData(text, frame);

    if (!d->document)
        return frame;

    const int bufferPlaneYOffset = d->bufferYOffset * frame->linesize[0];
    const int bufferUVOffset = d->bufferYOffset * frame->linesize[1] / 2;

    if (d->checkHash)
    {
        const qint64 hash = d->calculateHash(
            frame->data[0] + bufferPlaneYOffset,
            d->textImage->size(),
            frame->linesize[0]);

        if (hash == d->hash)
            return frame;
    }

    // Copy and convert frame buffer to image.
    yuv420_argb32_simd_intr(
        d->imageBuffer.data(),
        frame->data[0] + bufferPlaneYOffset,
        frame->data[1] + bufferUVOffset,
        frame->data[2] + bufferUVOffset,
        d->textImage->width(), d->textImage->height(),
        d->textImage->bytesPerLine(),
        frame->linesize[0], frame->linesize[1], 255);

    QPainter p(d->textImage.data());
    p.setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);
    d->document->drawContents(&p, d->textImage->rect());

    // Copy and convert RGBA32 image back to frame buffer.
    bgra_to_yv12_simd_intr(
        d->imageBuffer.data(),
        d->textImage->bytesPerLine(),
        frame->data[0]+bufferPlaneYOffset,
        frame->data[1]+bufferUVOffset,
        frame->data[2]+bufferUVOffset,
        frame->linesize[0], frame->linesize[1],
        d->textImage->width(), d->textImage->height(),
        false);

    if (!d->checkHash)
        return frame;

    d->hash = d->calculateHash(
        frame->data[0]+bufferPlaneYOffset,
        d->textImage->size(),
        frame->linesize[0]);

    return frame;
}

QSize TextImageFilter::updatedResolution(const QSize& srcSize)
{
    return srcSize;
}

} // namespace nx::vms::common::transcoding

#endif // ENABLE_DATA_PROVIDERS
