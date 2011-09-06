#include "imagebuttonitem.h"
#include "abstractbuttonitem_p.h"

#include <QtGui/QPainter>

class ImageButtonItemPrivate : public AbstractButtonItemPrivate
{
    Q_DECLARE_PUBLIC(ImageButtonItem)

public:
    ImageButtonItemPrivate(ImageButtonItem *qq) : AbstractButtonItemPrivate(qq), hovered(false) {}

public:
    QPixmap pixmaps[ImageButtonItem::NGroups][ImageButtonItem::Nroles];
    bool hovered;

    QPixmap getPixmap(ImageButtonItem::PixmapGroup group);
};

QPixmap ImageButtonItemPrivate::getPixmap(ImageButtonItem::PixmapGroup group)
{
    Q_Q(ImageButtonItem);

    QPixmap pix;

    if (q->isDown())
        pix = pixmaps[group][ImageButtonItem::Pressed];
    else if (q->isChecked())
        pix = pixmaps[group][ImageButtonItem::Checked];
    else if (q->hasFocus())
        pix = pixmaps[group][ImageButtonItem::Focus];
    else if (hovered)
        pix = pixmaps[group][ImageButtonItem::Hovered];
    else
        pix = pixmaps[group][ImageButtonItem::Background];

    if (pix.isNull())
        pix = pixmaps[group][ImageButtonItem::Background];

    if (pix.isNull())
        pix = pixmaps[ImageButtonItem::Active][ImageButtonItem::Background];

    return pix;
}


ImageButtonItem::ImageButtonItem(QGraphicsWidget *parent) :
    AbstractButtonItem(*new ImageButtonItemPrivate(this), parent)
{
    setAcceptsHoverEvents(true);
}

ImageButtonItem::ImageButtonItem(AbstractButtonItemPrivate &dd, QGraphicsWidget *parent) :
    AbstractButtonItem(dd, parent)
{
    setAcceptsHoverEvents(true);
}

void ImageButtonItem::addPixmap(const QPixmap &pix, ImageButtonItem::PixmapGroup group, ImageButtonItem::PixmapRole role)
{
    d_func()->pixmaps[group][role] = pix;
}

void ImageButtonItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QPixmap pix = d_func()->getPixmap(isEnabled() ? Active : Disabled);

    painter->drawPixmap(rect(), pix, QRectF(pix.rect()));
}

void ImageButtonItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    d_func()->hovered = true;
    AbstractButtonItem::hoverEnterEvent(event);
    update();
}

void ImageButtonItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    d_func()->hovered = false;
    AbstractButtonItem::hoverLeaveEvent(event);
    update();
}
