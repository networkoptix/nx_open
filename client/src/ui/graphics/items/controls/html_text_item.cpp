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

//    const int maximumCaptionLength = 64;
//    const int maximumDescriptionLength = 256;
//    const int radius = 4;
//    const int captionFontSize = 16;
//    const int descriptionFontSize = 12;
}

///
QnHtmlTextItemOptions::QnHtmlTextItemOptions()
    : backgroundColor()
    , maxWidth(kDefaultItemWidth)
    , borderRadius(0)
    , padding(kDefaultPadding)
    , autosize(false)
{
}

QnHtmlTextItemOptions::QnHtmlTextItemOptions(const QColor &initBackgroundColor
    , bool initAutosize
    , int initBorderRadius
    , int initPadding
    , int initMaxWidth)

    : backgroundColor(initBackgroundColor)
    , maxWidth(initMaxWidth)
    , borderRadius(initBorderRadius)
    , padding(initPadding)
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

    const int maxTextWidth = (options.maxWidth - options.padding * 2);

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
    else
        td.adjustSize();

    td.defaultTextOption().setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    pixmap = QPixmap(td.documentLayout()->documentSize().width() + options.padding * 2
        , td.documentLayout()->documentSize().height() + options.padding * 2);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    painter.setBrush(options.backgroundColor);
    painter.setPen(Qt::NoPen);

    {
        const QnScopedPainterAntialiasingRollback antialiasing(&painter, true);
        painter.drawRoundedRect(pixmap.rect(), options.borderRadius, options.borderRadius);
    }

    painter.translate(options.padding, options.padding);
    td.drawContents(&painter);
    painter.restore();

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
   // const auto policy = (options.autosize ? QSizePolicy::Maximum : QSizePolicy::Fixed);
    const auto policy = QSizePolicy::Maximum;
    setSizePolicy(policy, policy);
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
