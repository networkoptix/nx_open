#ifndef QN_TOOL_TIP_SLIDER_H
#define QN_TOOL_TIP_SLIDER_H

#include <QtCore/QBasicTimer>
#include <QtCore/QScopedPointer>

#include <ui/common/tool_tip_queryable.h>
#include <ui/animation/animated.h>
#include <ui/graphics/items/standard/graphics_slider.h>

class VariantAnimator;

class QnToolTipItem;
class QnToolTipSliderAnimationListener;

/**
 * Graphics slider that shows a balloon-shaped tooltip on top of its handle.
 */
class QnToolTipSlider: public Animated<GraphicsSlider>, public ToolTipQueryable {
    Q_OBJECT;

    typedef Animated<GraphicsSlider> base_type;

public:
    explicit QnToolTipSlider(QGraphicsItem *parent = NULL);
    virtual ~QnToolTipSlider();

    QnToolTipItem *toolTipItem() const;
    void setToolTipItem(QnToolTipItem *toolTipItem);

    bool isToolTipAutoHidden() const;
    void setAutoHideToolTip(bool autoHideToolTip);

    virtual bool eventFilter(QObject *target, QEvent *event) override;

protected:
    QnToolTipSlider(GraphicsSliderPrivate &dd, QGraphicsItem *parent);

    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

    virtual QString toolTipAt(const QPointF &pos) const override;

private:
    void init();

    void hideToolTip();
    void showToolTip();
    
    void updateToolTipVisibility();
    void updateToolTipPosition();
    void updateToolTipOpacity();
    void updateToolTipText();

private:
    friend class QnToolTipSliderAnimationListener;
    friend class QnToolTipSliderVisibilityAccessor;

    QScopedPointer<QnToolTipSliderAnimationListener> m_animationListener;
    QnToolTipItem *m_toolTipItem;
    VariantAnimator *m_toolTipItemVisibilityAnimator;
    qreal m_toolTipItemVisibility;
    QBasicTimer m_hideTimer;
    bool m_autoHideToolTip;
    bool m_sliderUnderMouse;
    bool m_toolTipUnderMouse;
    QPointF m_dragOffset;
};

#endif // QN_TOOL_TIP_SLIDER_H
