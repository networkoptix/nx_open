// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_image_filter.h"

#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>
#include <QtGui/QTextFrame>

#include <core/resource/resource_media_layout.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/yuvconvert.h>
#include <nx/utils/math/math.h>

namespace {

using namespace nx::core::transcoding;

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
    const TextImageFilter::TextGetter& textGetter,
    const CLVideoDecoderOutputPtr& frame,
    const TextImageFilter::Factor& factor,
    const int initialPixelSize)
{
    const QString initialText = textGetter(frame, /*cutSymbolsCount*/ 0);

    QFont font;
    font.setBold(true);
    font.setPixelSize(qMax(kMinTextHeight, initialPixelSize));

    const DocumentPtr document(new QTextDocument());
    const auto frameSize = frame->size();
    const int frameWidth = frameSize.width();
    const int offset = frameWidth * (1 - factor.x());
    updateDocumentFont(document, font, textAlignment, offset);

    QTextCursor cursor(document.data());
    cursor.setBlockFormat(getBlockFormat(textAlignment));
    cursor.setBlockCharFormat(getCharFormat());
    cursor.insertText(initialText);

    // Looking for the most appropriate font size which fits all lines without word wrapping.
    while (font.pixelSize() > kMinTextHeight && document->idealWidth() > frameWidth)
    {
        font.setPixelSize(font.pixelSize() - 1);
        updateDocumentFont(document, font, textAlignment, offset);
    }

    // Trying to fit overall text height to the frame size, or cut it.
    document->setTextWidth(frameWidth);
    int cutSymbolsCount = 0;
    int maxTextHeight = frameSize.height() * factor.y();
    while (document->size().height() > maxTextHeight && document->toPlainText().length() > 0)
    {
        cursor.select(QTextCursor::Document);
        cursor.removeSelectedText();
        cursor.insertText(textGetter(frame, ++cutSymbolsCount));
    }

    return document;
}

} // namespace

namespace nx::core::transcoding {

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
    QString sourceText;
    DocumentPtr document;
    QSharedPointer<QImage> textImage;
    QSharedPointer<quint8> imageBuffer;

    int bufferYOffset = 0;
    Factor factor = Factor(1, 1);

    Private(
        const VideoLayoutPtr& videoLayout,
        const Qt::Corner corner,
        const TextGetter& textGetter,
        const Factor sizeFactor);

    void updateTextData(const CLVideoDecoderOutputPtr& frame);

    qint64 calculateHash(
        const quint8* data,
        const QSize& size,
        int linesize);
};

TextImageFilter::Private::Private(
    const VideoLayoutPtr& videoLayout,
    const Qt::Corner corner,
    const TextGetter& textGetter,
    const Factor sizeFactor)
    :
    textGetter(textGetter),
    corner(corner),
    checkHash(videoLayout && videoLayout->channelCount() > 1),
    factor(sizeFactor)
{
}

void TextImageFilter::Private::updateTextData(const CLVideoDecoderOutputPtr& frame)
{
    frameSize = frame ? frame->size() : QSize();
    if (!frame)
    {
        document.reset();
        return;
    }

    const int initialTextHeight = frameSize.height() / kMaxTextLinesInFrame;
    const auto align = getAlignFromCorner(corner);
    document = makeDocument(align, textGetter, frame, factor, initialTextHeight);

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
    const TextGetter& textGetter,
    const Factor& factor)
{
    return QnAbstractImageFilterPtr(
        new TextImageFilter(videoLayout, corner, textGetter, factor));
}

TextImageFilter::TextImageFilter(
    const VideoLayoutPtr& videoLayout,
    const Qt::Corner corner,
    const TextGetter& textGetter,
    const Factor& factor)
    :
    d(new Private(videoLayout, corner, textGetter, factor))
{
}

TextImageFilter::~TextImageFilter()
{
}

CLVideoDecoderOutputPtr TextImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr&)
{
    if (!frame)
        return frame;

    const auto& text = d->textGetter(frame, /*cutSymbolsCount*/ 0);
    const auto& frameSize = frame->size();
    if (text != d->sourceText || frameSize != d->frameSize)
    {
        d->sourceText = text;
        d->updateTextData(frame);
    }

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
        QPainter::Antialiasing | QPainter::TextAntialiasing);
    d->document->drawContents(&p);

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

} // namespace nx::core::transcoding
