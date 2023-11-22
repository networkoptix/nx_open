// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "html_text_item.h"

#include <QtCore/QPointer>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>

#include <core/resource/camera_bookmark.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace nx::vms::client::desktop;

QnHtmlTextItemOptions::QnHtmlTextItemOptions(const QColor &initBackgroundColor
    , bool initAutosize
    , int initBorderRadius
    , int initHorPadding
    , int initVertPadding
    , int initMaxWidth)

    : backgroundColor(initBackgroundColor)
    , maxWidth(initMaxWidth)
    , borderRadius(initBorderRadius)
    , horPadding(initHorPadding)
    , vertPadding(initVertPadding)
    , autosize(initAutosize)
{
}

///

class QnHtmlTextItemPrivate: public QObject
{
    Q_DECLARE_PUBLIC(QnHtmlTextItem)
    QnHtmlTextItem *q_ptr;

public:
    QnHtmlTextItemPrivate(const QnHtmlTextItemOptions &options, QnHtmlTextItem *parent);

    QnHtmlTextItemOptions options;
    QString html;
    QPixmap icon;

    QPixmap pixmap;

    void updatePixmap();
};

QnHtmlTextItemPrivate::QnHtmlTextItemPrivate(const QnHtmlTextItemOptions &options
    , QnHtmlTextItem *parent)
    : q_ptr(parent)
    , options(options)
    , html()
    , pixmap()
{
    installEventHandler(parent, QEvent::PaletteChange, this,
        [this]()
        {
            if (!this->options.backgroundColor.isValid())
                updatePixmap();
        });
}

void QnHtmlTextItemPrivate::updatePixmap() {
    Q_Q(QnHtmlTextItem);

    if (html.isEmpty() && icon.size().isEmpty())
    {
        q->setMinimumSize(QSize());
        q->setMaximumSize(QSize());
        return;
    }

    const int maxTextWidth = (options.maxWidth - options.horPadding * 2);

    QTextDocument td;
    td.setDefaultFont(fontConfig()->normal());
    td.setDocumentMargin(0);
    td.setIndentWidth(0);
    td.setHtml(html);

    const auto docWidth = td.documentLayout()->documentSize().width();
    if ((docWidth > maxTextWidth) || !options.autosize)
        td.setTextWidth(maxTextWidth);

    td.defaultTextOption().setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    static const auto ratio = qApp->devicePixelRatio();
    auto iconSize = icon.size() / ratio;
    auto width = td.documentLayout()->documentSize().width() + options.horPadding * 2;
    auto height = td.documentLayout()->documentSize().height() + options.vertPadding * 2;

    if (!iconSize.isEmpty())
    {
        width += (iconSize.width() + options.horSpacing);
    }

    const auto baseSize = html.isEmpty()
        ? iconSize + QSize(options.horPadding * 2, options.vertPadding * 2)
        : QSize(width, height);
    const auto pixmapSize = baseSize * ratio;

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
        if (html.isEmpty())
            offset = options.vertPadding;
        painter.translate(0, offset);
        painter.drawPixmap(0, 0, icon);
        painter.translate(options.horSpacing + iconSize.width(), -offset);
    }

    painter.translate(0, options.vertPadding);
    td.drawContents(&painter);
    q->setMinimumSize(baseSize);
    q->setMaximumSize(baseSize);
    q->update();
}

///
QnHtmlTextItem::QnHtmlTextItem(const QString &html
    , const QnHtmlTextItemOptions &options
    , QGraphicsItem *parent)
    : base_type(parent)
    , d_ptr(new QnHtmlTextItemPrivate(options, this))
{
    setHtml(html);
    setAcceptedMouseButtons(Qt::NoButton);
}

QnHtmlTextItem::~QnHtmlTextItem()
{
}

void QnHtmlTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget);

    Q_D(QnHtmlTextItem);
    paintPixmapSharp(painter, d->pixmap);
}

QString QnHtmlTextItem::html() const {
    Q_D(const QnHtmlTextItem);
    return d->html;
}

void QnHtmlTextItem::setHtml(const QString &html)
{
    Q_D(QnHtmlTextItem);
    if (d->html == html)
        return;
    d->html = html;
    d->updatePixmap();
}

void QnHtmlTextItem::setIcon(const QPixmap& icon)
{
    Q_D(QnHtmlTextItem);
    // No pixmap comparison here for the early return.
    // Since QPixmap hasn't operator== defined, pixmaps will be implicitely converted to the
    // QCursor instances to perform comparison. This is not what was expected in general and also
    // may lead to a lot of warnings reported which even may cause significant performance drop.
    if (d->icon.isNull() && icon.isNull())
        return;
    d->icon = icon;
    d->updatePixmap();
}

QnHtmlTextItemOptions QnHtmlTextItem::options() const {
    Q_D(const QnHtmlTextItem);
    return d->options;
}

void QnHtmlTextItem::setOptions(const QnHtmlTextItemOptions &options) {
    Q_D(QnHtmlTextItem);
    if (d->options == options)
        return;
    d->options = options;
    d->updatePixmap();
}
