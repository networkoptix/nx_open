#pragma once

#include "graphics_widget.h"

class GraphicsPixmapPrivate;
class GraphicsPixmap : public GraphicsWidget {
    Q_OBJECT

    typedef GraphicsWidget base_type;

public:
    GraphicsPixmap(QGraphicsItem *parent = 0);
    GraphicsPixmap(const QPixmap &pixmap, QGraphicsItem *parent = 0);
    ~GraphicsPixmap();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    QPixmap pixmap() const;
    void setPixmap(const QPixmap &pixmap);

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

private:
    QScopedPointer<GraphicsPixmapPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(GraphicsPixmap)
};
