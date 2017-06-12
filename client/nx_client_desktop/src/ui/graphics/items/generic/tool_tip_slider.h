#pragma once

#include <QtCore/QBasicTimer>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <ui/common/tool_tip_queryable.h>
#include <ui/animation/animated.h>
#include <ui/graphics/items/standard/graphics_slider.h>

class VariantAnimator;

class QnToolTipWidget;
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

    QnToolTipWidget *toolTipItem() const;
    void setToolTipItem(QnToolTipWidget *toolTipItem);

    bool isToolTipAutoHidden() const;
    void setAutoHideToolTip(bool autoHideToolTip);

    qreal tooltipMargin() const;
    void setTooltipMargin(qreal margin);

    virtual bool eventFilter(QObject *target, QEvent *event) override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void hideToolTip(bool animated = true);
    void showToolTip(bool animated = true);

    void setToolTipEnabled(bool enabled);

    int toolTipHideDelayMs() const;
    void setToolTipHideDelayMs(int delayMs);

protected :
    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

    virtual QString toolTipAt(const QPointF &pos) const override;
    virtual bool showOwnTooltip(const QPointF &pos) override;

private:
    void updateToolTipVisibility();
    Q_SLOT void updateToolTipPosition();
    void updateToolTipOpacity();
    void updateToolTipText();

private:
    friend class QnToolTipSliderAnimationListener;
    friend class QnToolTipSliderVisibilityAccessor;

    QScopedPointer<QnToolTipSliderAnimationListener> m_animationListener;
    QPointer<QnToolTipWidget> m_tooltipWidget;
    VariantAnimator* m_tooltipWidgetVisibilityAnimator;
    qreal m_tooltipWidgetVisibility;
    QBasicTimer m_hideTimer;
    bool m_autoHideToolTip;
    bool m_sliderUnderMouse;
    bool m_toolTipUnderMouse;
    bool m_pendingPositionUpdate;
    bool m_instantPositionUpdate;
    QPointF m_dragOffset;
    qreal m_tooltipMargin;
    bool m_toolTipEnabled;
    int m_toolTipHideDelayMs;
};
