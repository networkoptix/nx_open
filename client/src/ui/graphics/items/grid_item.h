#ifndef QN_GRID_ITEM_H
#define QN_GRID_ITEM_H

#include <QGraphicsObject>
#include <QWeakPointer>

class QnWorkbenchGridMapper;
class QnVariantAnimator;

class QnGridItem : public QGraphicsObject {
    Q_OBJECT;
    Q_PROPERTY(QColor color READ color WRITE setColor);
    Q_PROPERTY(QColor defaultColor READ defaultColor WRITE setDefaultColor);

public:
    QnGridItem(QGraphicsItem *parent = NULL);
    
    virtual ~QnGridItem();

    virtual QRectF boundingRect() const override;

    QnWorkbenchGridMapper *mapper() const {
        return m_mapper.data();
    }

    void setMapper(QnWorkbenchGridMapper *mapper);

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

    const QColor &defaultColor() const {
        return m_defaultColor;
    }

    void setDefaultColor(const QColor &defaultColor) {
        m_defaultColor = defaultColor;
    }

    qreal lineWidth() const {
        return m_lineWidth;
    }

    void setLineWidth(qreal lineWidth) {
        m_lineWidth = lineWidth;
    }

    void fadeIn();

    void fadeOut();

    void setFadingSpeed(qreal speed);

    // animates visibility changing
    //void setVisibleAnimated(bool visible, int time_ms = 1000);
    //inline void showAnimated(int time_ms = 1000) { setVisibleAnimated(true, time_ms); }
    //inline void hideAnimated(int time_ms = 1000) { setVisibleAnimated(false, time_ms); }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF m_boundingRect;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    QColor m_color;
    QColor m_defaultColor;
    qreal m_lineWidth;

    QnVariantAnimator *m_colorAnimator;
};


#endif // QN_GRID_ITEM_H
