#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <QGraphicsWidget>
#include <QPixmap>
#include "clickable.h"

class QnImageButtonWidget: public Clickable<QGraphicsWidget> {
    Q_OBJECT;

    typedef Clickable<QGraphicsWidget> base_type;

public:
    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    void setPixmap(const QPixmap &pixmap);

    const QPixmap &pixmap() const {
        return m_pixmap;
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void clicked();

protected:
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;

private:
    QPixmap m_pixmap;
};


#endif // QN_IMAGE_BUTTON_WIDGET_H
