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
    virtual ~QnToolTipSlider();

    QnToolTipItem *toolTipItem() const;
    void setToolTipItem(QnToolTipItem *toolTipItem);

    bool isToolTipAutoHidden() const;
    void setAutoHideToolTip(bool autoHideToolTip);

    virtual bool sceneEventFilter(QGraphicsItem *target, QEvent *event) override;

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void hideToolTip();
    void showToolTip();
    
    void updateToolTipVisibility();

private:
    QnToolTipItem *m_toolTipItem;
    QBasicTimer m_hideTimer;
    bool m_autoHideToolTip;
    bool m_sliderUnderMouse;
    bool m_toolTipUnderMouse;
    qreal m_dragOffset;
};

#endif // QN_TOOL_TIP_SLIDER_H
