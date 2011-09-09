#ifndef IMAGEBUTTONITEM_H
#define IMAGEBUTTONITEM_H

#include "abstractbuttonitem.h"

class ImageButtonItemPrivate;
class ImageButtonItem : public AbstractButtonItem
{
    Q_OBJECT

public:
    enum PixmapRole {
        Background,
        Hovered,
        Checked,
        Pressed,
        Focus,
        Nroles
    };
    enum PixmapGroup {
        Active,
        Disabled,
        Inactive,
        NGroups
    };
    explicit ImageButtonItem(QGraphicsItem *parent = 0);

    void addPixmap(const QPixmap &pix, PixmapGroup group, PixmapRole role);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
    explicit ImageButtonItem(AbstractButtonItemPrivate &dd, QGraphicsItem *parent = 0);

private:
    Q_DECLARE_PRIVATE(ImageButtonItem)
};

#endif // IMAGEBUTTONITEM_H
