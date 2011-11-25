#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <QGraphicsWidget>
#include <QPixmap>

class QnImageButtonWidget: public QGraphicsWidget {
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    void setPixmap(const QPixmap &pixmap);

    const QPixmap &pixmap() const {
        return m_pixmap;
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPixmap m_pixmap;
};


#endif // QN_IMAGE_BUTTON_WIDGET_H
