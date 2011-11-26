#include "imagebutton.h"

#include <QtGui/QPainter>

#include "ui/widgets2/abstractgraphicsbutton_p.h"

class ImageButtonPrivate : public AbstractGraphicsButtonPrivate
{
    Q_DECLARE_PUBLIC(ImageButton)

public:
    ImageButtonPrivate() : AbstractGraphicsButtonPrivate() {}

    QPixmap getPixmap(ImageButton::PixmapGroup group);

private:
    QPixmap pixmaps[ImageButton::NGroups][ImageButton::Nroles];
};

QPixmap ImageButtonPrivate::getPixmap(ImageButton::PixmapGroup group)
{
    Q_Q(ImageButton);

    QPixmap pix;

    QStyleOption opt;
    q->initStyleOption(&opt);

    if (q->isDown())
        pix = pixmaps[group][ImageButton::Pressed];
    else if (q->isChecked())
        pix = pixmaps[group][ImageButton::Checked];
    else if (q->hasFocus() && !pixmaps[group][ImageButton::Focus].isNull())
        pix = pixmaps[group][ImageButton::Focus];
    else if ((opt.state & QStyle::State_MouseOver) && !pixmaps[group][ImageButton::Hovered].isNull())
        pix = pixmaps[group][ImageButton::Hovered];
    else
        pix = pixmaps[group][ImageButton::Background];

    if (pix.isNull())
        pix = pixmaps[group][ImageButton::Background];

    if (pix.isNull())
        pix = pixmaps[ImageButton::Active][ImageButton::Background];

    return pix;
}


ImageButton::ImageButton(QGraphicsItem *parent) :
    AbstractGraphicsButton(*new ImageButtonPrivate, parent)
{
}

ImageButton::ImageButton(AbstractGraphicsButtonPrivate &dd, QGraphicsItem *parent) :
    AbstractGraphicsButton(dd, parent)
{
}

void ImageButton::addPixmap(const QPixmap &pix, ImageButton::PixmapGroup group, ImageButton::PixmapRole role)
{
    d_func()->pixmaps[group][role] = pix;
}

void ImageButton::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QPixmap pix = d_func()->getPixmap(isEnabled() ? Active : Disabled);

    painter->drawPixmap(rect(), pix, QRectF(pix.rect()));
}
