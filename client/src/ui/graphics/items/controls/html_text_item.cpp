#include "html_text_item.h"

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>

#include <core/resource/camera_bookmark.h>
#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

namespace 
{
    enum
    {
        kDefaultItemWidth = 250
        , kDefaultPadding = 8
    };
}

///

QnHtmlTextItemOptions::QnHtmlTextItemOptions()
    : backgroundColor()
    , maxWidth(kDefaultItemWidth)
    , borderRadius(0)
    , horPadding(kDefaultPadding)
    , vertPadding(kDefaultPadding)
    , autosize(false)
{
}

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

class QnHtmlTextItemPrivate 
{
    Q_DECLARE_PUBLIC(QnHtmlTextItem)
    QnHtmlTextItem *q_ptr;

public:
    QnHtmlTextItemPrivate(const QnHtmlTextItemOptions &options
        , QnHtmlTextItem *parent);

    QnHtmlTextItemOptions options;
    QString html;
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

    pixmap = QPixmap(td.documentLayout()->documentSize().width() + options.horPadding * 2
        , td.documentLayout()->documentSize().height() + options.vertPadding * 2);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    painter.setBrush(options.backgroundColor);
    painter.setPen(Qt::NoPen);

    {
        const QnScopedPainterAntialiasingRollback antialiasing(&painter, true);
        painter.drawRoundedRect(pixmap.rect(), options.borderRadius, options.borderRadius);
    }

    painter.translate(options.horPadding, options.vertPadding);
    td.drawContents(&painter);

    q->resize(QSizeF(pixmap.size()));
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
}

QnHtmlTextItem::~QnHtmlTextItem() 
{
}

void QnHtmlTextItem::setHtml(const QString &html)
{
    Q_D(QnHtmlTextItem);
    
    const bool updatePixmap = (d->html != html);
    d->html = html;

    if (updatePixmap)
        d->updatePixmap();
}

void QnHtmlTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget);

    Q_D(QnHtmlTextItem);

    if (!d->pixmap.isNull())
        painter->drawPixmap(0, 0, d->pixmap);
}
