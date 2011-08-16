#ifndef IMAGEBUTTONITEM_H
#define IMAGEBUTTONITEM_H

#include "abstractbuttonitem.h"
#include <QPalette>

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
    explicit ImageButtonItem(QGraphicsWidget *parent = 0);

    void addPixmap(const QPixmap &pix, PixmapGroup group, PixmapRole role);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

signals:

public slots:

protected:
    explicit ImageButtonItem(AbstractButtonItemPrivate &dd, QGraphicsWidget *parent = 0);
    Q_DECLARE_PRIVATE(ImageButtonItem)
};

#endif // IMAGEBUTTONITEM_H
