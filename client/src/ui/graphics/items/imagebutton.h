#ifndef IMAGEBUTTON_H
#define IMAGEBUTTON_H

#include <ui/graphics/items/standard/abstractgraphicsbutton.h>

class ImageButtonPrivate;
class ImageButton : public AbstractGraphicsButton
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
    explicit ImageButton(QGraphicsItem *parent = 0);

    void addPixmap(const QPixmap &pix, PixmapGroup group, PixmapRole role);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
    explicit ImageButton(AbstractGraphicsButtonPrivate &dd, QGraphicsItem *parent = 0);

private:
    Q_DECLARE_PRIVATE(ImageButton)
};

#endif // IMAGEBUTTON_H
