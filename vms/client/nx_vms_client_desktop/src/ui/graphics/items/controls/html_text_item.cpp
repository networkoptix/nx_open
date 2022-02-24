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
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

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

    const int maxTextWidth = (options.maxWidth - options.horPadding * 2);

    static const QFont kDefaultFont = []()
    {
        QFont font;
        font.setPixelSize(13);
        return font;
    }();

    QTextDocument td;
    td.setDefaultFont(kDefaultFont);
    td.setDocumentMargin(0);
    td.setIndentWidth(0);
    td.setHtml(html);

    const auto docWidth = td.documentLayout()->documentSize().width();
    if ((docWidth > maxTextWidth) || !options.autosize)
        td.setTextWidth(maxTextWidth);

    td.defaultTextOption().setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    static const auto ratio = qApp->devicePixelRatio();
    auto width = td.documentLayout()->documentSize().width() + options.horPadding * 2;
    const auto height = td.documentLayout()->documentSize().height() + options.vertPadding * 2;
    auto iconSize = icon.size() / ratio;

    if (!iconSize.isEmpty())
        width += (iconSize.width() + options.horSpacing);

    const auto baseSize = QSize(width, height);
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
        painter.drawRoundedRect(QRectF(QPointF(), baseSize), options.borderRadius, options.borderRadius);
    }

    painter.translate(options.horPadding, options.vertPadding);
    if (!iconSize.isEmpty())
    {
        painter.drawPixmap(0, 0, icon);
        painter.translate(options.horSpacing + iconSize.width(), 0);
    }

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
    d->icon = icon;
    d->updatePixmap();
}

QnHtmlTextItemOptions QnHtmlTextItem::options() const {
    Q_D(const QnHtmlTextItem);
    return d->options;
}

void QnHtmlTextItem::setOptions( const QnHtmlTextItemOptions &options ) {
    Q_D(QnHtmlTextItem);
    if (d->options == options)
        return;
    d->options = options;
    d->updatePixmap();
}
