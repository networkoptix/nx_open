#ifndef GRID_RAISED_CONE_ITEM_H
#define GRID_RAISED_CONE_ITEM_H

#include <QObject>
#include <QGraphicsWidget>

class QnGridRaisedConeItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit QnGridRaisedConeItem(QGraphicsWidget *parent = NULL);
    virtual ~QnGridRaisedConeItem();

    virtual QRectF boundingRect() const override;

    /** Adjust source geometry */
    void adjustGeometry(QRectF oldGeometry);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void updateGeometry();

private:
    QRectF m_sourceRect;
    QRectF m_targetRect;
    QGraphicsWidget* m_raisedWidget;
    QRect m_sourceGeometry;
    qreal m_rotation;
};

/** Returns the separate raised cone item for each widget. Auto-creates if none. Deletes automatically. */
QnGridRaisedConeItem* raisedConeItem(QGraphicsWidget* widget);

Q_DECLARE_METATYPE(QnGridRaisedConeItem *)

#endif // GRID_RAISED_CONE_ITEM_H
