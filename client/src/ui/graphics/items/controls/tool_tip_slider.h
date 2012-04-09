#ifndef QN_TOOL_TIP_SLIDER_H
#define QN_TOOL_TIP_SLIDER_H

#include <QtCore/QBasicTimer>

#include <ui/graphics/items/standard/graphicsslider.h>

class QnToolTipItem;

class QnToolTipSlider: public GraphicsSlider {
    Q_OBJECT;

    typedef GraphicsSlider base_type;

public:
    explicit QnToolTipSlider(QGraphicsItem *parent = NULL);
    explicit QnToolTipSlider(Qt::Orientation orientation, QGraphicsItem *parent = NULL);
    virtual ~QnToolTipSlider();

    QnToolTipItem *toolTipItem() const;
    void setToolTipItem(QnToolTipItem *toolTipItem);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void timerEvent(QTimerEvent *event) override;

private:
    QnToolTipItem *m_toolTipItem;
    QBasicTimer m_hideTimer;
};

#endif // QN_TOOL_TIP_SLIDER_H
