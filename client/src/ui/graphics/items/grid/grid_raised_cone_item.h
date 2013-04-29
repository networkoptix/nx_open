#ifndef GRID_RAISED_CONE_ITEM_H
#define GRID_RAISED_CONE_ITEM_H

#include <QObject>
#include <QGraphicsObject>

class QnGridRaisedConeItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF viewportRect READ viewportRect WRITE setViewportRect)

public:
    explicit QnGridRaisedConeItem(QGraphicsItem *parent = NULL);
    virtual ~QnGridRaisedConeItem();

    virtual QRectF boundingRect() const override;

    const QRectF &viewportRect() const;
    void setViewportRect(const QRectF &rect);

    QGraphicsWidget* raisedWidget() const;
    void setRaisedWidget(QGraphicsWidget* widget, QRectF oldGeometry);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void updateGeometry();

private:
    QRectF m_sourceRect;
    QRectF m_rect;
    QGraphicsWidget* m_raisedWidget;
    QRect m_sourceGeometry;
};

#endif // GRID_RAISED_CONE_ITEM_H
