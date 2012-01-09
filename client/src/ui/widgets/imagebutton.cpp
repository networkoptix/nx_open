#include "imagebutton.h"

#include <QtGui/QPainter>

#include "ui/widgets2/abstractgraphicsbutton_p.h"
#include "utils/common/util.h"

class ImageButtonPrivate : public AbstractGraphicsButtonPrivate
{
    Q_DECLARE_PUBLIC(ImageButton)

public:
    ImageButtonPrivate() : AbstractGraphicsButtonPrivate(), isUnderMouse(false)
    {
    }

    // I have used reference (&) because of GL painter is cached Pixmaps in GL texture map, where key is internal Pixmap ID.
    QPixmap& getPixmap(ImageButton::PixmapGroup group);

private:
    QPixmap pixmaps[ImageButton::NGroups][ImageButton::Nroles];
    bool isUnderMouse;
};

QPixmap& ImageButtonPrivate::getPixmap(ImageButton::PixmapGroup group)
{
    Q_Q(ImageButton);


    if (q->isDown())
        return pixmaps[group][ImageButton::Pressed];
    else if (q->isChecked())
        return pixmaps[group][ImageButton::Checked];
    else if (q->hasFocus() && !pixmaps[group][ImageButton::Focus].isNull())
        return pixmaps[group][ImageButton::Focus];
    else if (isUnderMouse && !pixmaps[group][ImageButton::Hovered].isNull())
        return pixmaps[group][ImageButton::Hovered];
    else
        return pixmaps[group][ImageButton::Background];

    if (!pixmaps[group][ImageButton::Background].isNull())
        return pixmaps[group][ImageButton::Background];

    return pixmaps[ImageButton::Active][ImageButton::Background];

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
    //qint64 t = getUsecTimer();

    Q_UNUSED(option)
    Q_UNUSED(widget)

    QPixmap& pix = d_func()->getPixmap(isEnabled() ? Active : Disabled);


    painter->drawPixmap(rect(), pix, QRectF(pix.rect()));

    //qDebug() << "time=" << (getUsecTimer() - t)/1000.0;
}

void ImageButton::hoverEnterEvent ( QGraphicsSceneHoverEvent * )
{
    d_func()->isUnderMouse = true;
}

void ImageButton::hoverLeaveEvent ( QGraphicsSceneHoverEvent * )
{
    d_func()->isUnderMouse = false;
}
