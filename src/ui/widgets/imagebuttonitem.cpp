#include "imagebuttonitem.h"
#include "abstractbuttonitem_p.h"

#include <QPainter>

class ImageButtonItemPrivate : public AbstractButtonItemPrivate
{
public:
    ImageButtonItemPrivate(ImageButtonItem *qq) : AbstractButtonItemPrivate(qq) {}

public:
    QPixmap pixmaps[ImageButtonItem::NGroups][ImageButtonItem::Nroles];
    bool hovered;
};

ImageButtonItem::ImageButtonItem(QGraphicsWidget *parent) :
    AbstractButtonItem(*new ImageButtonItemPrivate(this), parent)
{
    d_func()->hovered = false;
    setAcceptsHoverEvents(true);
}

ImageButtonItem::ImageButtonItem(AbstractButtonItemPrivate &dd, QGraphicsWidget *parent) :
    AbstractButtonItem(dd, parent)
{
}

void ImageButtonItem::addPixmap(const QPixmap &pix, ImageButtonItem::PixmapGroup group, ImageButtonItem::PixmapRole role)
{
    Q_D(ImageButtonItem);

    d->pixmaps[group][role] = pix;
}

void ImageButtonItem::paint(QPainter *painter, const QStyleOptionGraphicsItem */*option*/, QWidget */*widget*/)
{
    Q_D(ImageButtonItem);

    QRectF r = rect();
    QPixmap pix;
    if (isDown())
        pix = d->pixmaps[Active][Pressed];
    else if (isChecked())
        pix = d->pixmaps[Active][Checked];
    else if (hasFocus())
        pix = d->pixmaps[Active][Focus];
    else if (d->hovered)
        pix = d->pixmaps[Active][Hovered];
    else
        pix = d->pixmaps[Active][Background];

    if (pix.isNull())
        pix = d->pixmaps[Active][Background];

    painter->fillRect(r, pix.scaled((int)r.width(), (int)r.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void ImageButtonItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    d_func()->hovered = true;
    AbstractButtonItem::hoverEnterEvent(event);
}

void ImageButtonItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    d_func()->hovered = false;
    AbstractButtonItem::hoverLeaveEvent(event);
    update();
}

