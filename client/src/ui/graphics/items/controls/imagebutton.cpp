#include "imagebutton.h"
#include <QtGui/QPainter>
#include <ui/graphics/items/standard/abstractgraphicsbutton_p.h>

class ImageButtonPrivate : public AbstractGraphicsButtonPrivate
{
    Q_DECLARE_PUBLIC(ImageButton)

public:
    ImageButtonPrivate() : AbstractGraphicsButtonPrivate() {}

    // I have used reference (&) because of GL painter is cached Pixmaps in GL texture map, where key is internal Pixmap ID.
    const QPixmap &getPixmap(ImageButton::PixmapGroup group) const;

private:
    QPixmap pixmaps[ImageButton::NGroups][ImageButton::Nroles];
};

const QPixmap &ImageButtonPrivate::getPixmap(ImageButton::PixmapGroup group) const
{
    Q_Q(const ImageButton);

    if (q->isDown())
        return pixmaps[group][ImageButton::Pressed];
    if (q->isChecked() && !pixmaps[group][ImageButton::Checked].isNull())
        return pixmaps[group][ImageButton::Checked];
    if (q->hasFocus() && !pixmaps[group][ImageButton::Focus].isNull())
        return pixmaps[group][ImageButton::Focus];
    if (isUnderMouse && !pixmaps[group][ImageButton::Hovered].isNull())
        return pixmaps[group][ImageButton::Hovered];

    if (pixmaps[group][ImageButton::Background].isNull())
        group = ImageButton::Active;

    return pixmaps[group][ImageButton::Background];
}


ImageButton::ImageButton(QGraphicsItem *parent)
    : AbstractGraphicsButton(*new ImageButtonPrivate, parent)
{
}

ImageButton::ImageButton(AbstractGraphicsButtonPrivate &dd, QGraphicsItem *parent)
    : AbstractGraphicsButton(dd, parent)
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

    const QPixmap &pix = d_func()->getPixmap(isEnabled() ? Active : Disabled);

    painter->drawPixmap(rect(), pix, QRectF(pix.rect()));
}
