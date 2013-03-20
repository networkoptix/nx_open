#ifndef GRID_BACKGROUND_ITEM_H
#define GRID_BACKGROUND_ITEM_H

#include <QObject>
#include <QGraphicsObject>

class QnWorkbenchGridMapper;

class QnGridBackgroundItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)

public:
    explicit QnGridBackgroundItem(QGraphicsItem *parent = NULL);
    virtual ~QnGridBackgroundItem();

    virtual QRectF boundingRect() const override;

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

    const QRect &sceneRect() const {
        return m_sceneRect;
    }

    void setSceneRect(const QRect &rect);

    QnWorkbenchGridMapper *mapper() const {
        return m_mapper.data();
    }

    void setMapper(QnWorkbenchGridMapper *mapper);
protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void updateGeometry();

private:
    QRect m_sceneRect;
    QRectF m_rect;
    QColor m_color;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
};


#endif // GRID_BACKGROUND_ITEM_H
