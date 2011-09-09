#include "imagebuttonitem.h"
#include "abstractbuttonitem_p.h"

#include <QtGui/QPainter>

class ImageButtonItemPrivate : public AbstractButtonItemPrivate
{
    Q_DECLARE_PUBLIC(ImageButtonItem)

public:
    ImageButtonItemPrivate() : AbstractButtonItemPrivate() {}

    QPixmap getPixmap(ImageButtonItem::PixmapGroup group);

private:
    QPixmap pixmaps[ImageButtonItem::NGroups][ImageButtonItem::Nroles];
};

QPixmap ImageButtonItemPrivate::getPixmap(ImageButtonItem::PixmapGroup group)
{
    Q_Q(ImageButtonItem);

    QPixmap pix;

    if (q->isDown())
        pix = pixmaps[group][ImageButtonItem::Pressed];
    else if (q->isChecked())
        pix = pixmaps[group][ImageButtonItem::Checked];
    else if (q->hasFocus() && !pixmaps[group][ImageButtonItem::Focus].isNull())
        pix = pixmaps[group][ImageButtonItem::Focus];
    else if (isUnderMouse && !pixmaps[group][ImageButtonItem::Hovered].isNull())
        pix = pixmaps[group][ImageButtonItem::Hovered];
    else
        pix = pixmaps[group][ImageButtonItem::Background];

    if (pix.isNull())
        pix = pixmaps[group][ImageButtonItem::Background];

    if (pix.isNull())
        pix = pixmaps[ImageButtonItem::Active][ImageButtonItem::Background];

    return pix;
}


ImageButtonItem::ImageButtonItem(QGraphicsItem *parent) :
    AbstractButtonItem(*new ImageButtonItemPrivate, parent)
{
}

ImageButtonItem::ImageButtonItem(AbstractButtonItemPrivate &dd, QGraphicsItem *parent) :
    AbstractButtonItem(dd, parent)
{
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
