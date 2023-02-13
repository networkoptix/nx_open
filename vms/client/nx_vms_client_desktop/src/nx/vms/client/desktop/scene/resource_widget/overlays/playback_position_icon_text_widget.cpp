// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "playback_position_icon_text_widget.h"

#include <QtCore/QPointer>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>

#include <nx/utils/string.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

struct PlaybackPositionIconTextWidget::Private: public QObject
{
    Q_DECLARE_PUBLIC(PlaybackPositionIconTextWidget)
    PlaybackPositionIconTextWidget* q_ptr;

public:
    Private(const PlaybackPositionIconTextWidgetOptions& options,
        PlaybackPositionIconTextWidget* parent);

    PlaybackPositionIconTextWidgetOptions options;
    QString text;
    /**
     *  Icon that should be drawn on the left from text. If there is no text it is drawn in
     *  the center of the widget.
     */
    QPixmap iconPixmap;

    /** Cached contents of the item. */
    QPixmap pixmap;

    void updatePixmap();
};

PlaybackPositionIconTextWidget::Private::Private(
    const PlaybackPositionIconTextWidgetOptions& options,
    PlaybackPositionIconTextWidget* parent)
    :
    q_ptr(parent),
    options(options)
{
    installEventHandler(parent,
        QEvent::PaletteChange,
        this,
        [this]()
        {
            if (!this->options.backgroundColor.isValid())
                updatePixmap();
        });
}

void PlaybackPositionIconTextWidget::Private::updatePixmap()
{
    Q_Q(PlaybackPositionIconTextWidget);

    if (text.isEmpty() && iconPixmap.size().isEmpty())
    {
        q->setMinimumSize(QSize());
        q->setMaximumSize(QSize());
        return;
    }

    // Calculating size.
    const int maxTextWidth = (options.maxWidth - options.horPadding * 2);

    static const QFont kDefaultFont =
        []()
        {
            QFont font = QFont("Roboto");
            font.setPixelSize(12);
            return font;
        }();

    QTextDocument td;
    td.setDefaultFont(kDefaultFont);
    td.setDocumentMargin(0);
    td.setIndentWidth(0);
    td.setHtml(text);

    const auto docWidth = td.documentLayout()->documentSize().width();
    if (docWidth > maxTextWidth)
        td.setTextWidth(maxTextWidth);

    td.defaultTextOption().setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    static const auto ratio = qApp->devicePixelRatio();
    auto iconSize = iconPixmap.size() / ratio;

    auto height = (options.fixedHeight != 0)
        ? options.fixedHeight
        : text.isEmpty()
            ? iconPixmap.height() + options.vertPadding * 2
            : td.documentLayout()->documentSize().height() + options.vertPadding * 2;

    int width = 0;
    if (iconPixmap.size().isEmpty())
    {
        width = td.documentLayout()->documentSize().width() + options.horPadding * 2;
    }
    else
    {
        if (text.isEmpty())
        {
            width = iconSize.width() + options.horPadding * 2;
        }
        else
        {
            width = td.documentLayout()->documentSize().width() + iconSize.width()
                + options.horPadding * 2 + options.horSpacing;
        }
    }

    const auto baseSize = QSize(width, height);
    const auto pixmapSize = baseSize * ratio;
    // End calculating size.

    pixmap = QPixmap(pixmapSize);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    const auto backgroundColor = options.backgroundColor.isValid()
        ? options.backgroundColor
        : q->palette().color(QPalette::Window);

    QPainter painter(&pixmap);
    painter.setBrush(backgroundColor);
    painter.setPen(Qt::NoPen);

    {
        const QnScopedPainterAntialiasingRollback antialiasing(&painter, true);
        painter.drawRoundedRect(
            QRectF(QPointF(), baseSize), options.borderRadius, options.borderRadius);
    }

    painter.translate(options.horPadding, 0);
    if (!iconSize.isEmpty())
    {
        auto offset = (height - iconSize.height()) / 2;
        painter.translate(0, offset);
        painter.drawPixmap(0, 0, iconPixmap);
        painter.translate(options.horSpacing + iconSize.width(), -offset);
    }

    if (!text.isEmpty())
    {
        painter.translate(0, options.vertPadding);
        td.drawContents(&painter);
    }
    q->setMinimumSize(baseSize);
    q->setMaximumSize(baseSize);
    q->update();
}

PlaybackPositionIconTextWidget::PlaybackPositionIconTextWidget(
    const QString& text,
    const PlaybackPositionIconTextWidgetOptions& options,
    QGraphicsItem* parent):
    base_type(parent), d(new Private(options, this))
{
    setText(text);
    setAcceptedMouseButtons(Qt::NoButton);
}

PlaybackPositionIconTextWidget::~PlaybackPositionIconTextWidget()
{
}

void PlaybackPositionIconTextWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    paintPixmapSharp(painter, d->pixmap);
}

QString PlaybackPositionIconTextWidget::text() const
{
    return d->text;
}

void PlaybackPositionIconTextWidget::setText(const QString& text)
{
    if (d->text == text)
        return;
    d->text = text;
    d->updatePixmap();
}

void PlaybackPositionIconTextWidget::setIcon(const QPixmap& icon)
{
    // No pixmap comparison here for the early return.
    // Since QPixmap hasn't operator== defined, pixmaps will be implicitely converted to the
    // QCursor instances to perform comparison. This is not what was expected in general and also
    // may lead to a lot of warnings reported which even may cause significant performance drop.
    if (d->iconPixmap.isNull() && icon.isNull())
        return;
    d->iconPixmap = icon;
    d->updatePixmap();
}

PlaybackPositionIconTextWidgetOptions PlaybackPositionIconTextWidget::options() const
{
    return d->options;
}

void PlaybackPositionIconTextWidget::setOptions(
    const PlaybackPositionIconTextWidgetOptions& options)
{
    if (d->options == options)
        return;
    d->options = options;
    d->updatePixmap();
}
