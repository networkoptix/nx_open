#pragma once

#include <QtCore/QElapsedTimer>
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
class QnToolTipSlider: public Animated<GraphicsSlider>, public ToolTipQueryable
{
    Q_OBJECT

    using base_type = Animated<GraphicsSlider>;

public:
    explicit QnToolTipSlider(QGraphicsItem* parent = nullptr);
    virtual ~QnToolTipSlider();

    /**
     * Item to display tooltip. Note: this class takes ownership of an item.
     */
    QnToolTipWidget* toolTipItem() const;
    void setToolTipItem(QnToolTipWidget* toolTipItem);

    /**
     * Auto-hide behavior (enabled by default). Working the following way: once a certain event
     * is triggered (by default: hover enter, hover leave), ::calculateToolTipAutoVisibility() is
     * called. If value differs from the last calculated, tooltip is displayed or hidden. Before
     * each animation setupShowAnimator() or setupHideAnimator() functions are called.
     * Also tooltip is displayed for some time if slider value was changed recently (2500ms).
     */
    bool isToolTipAutoHidden() const;
    void setAutoHideToolTip(bool autoHideToolTip);

    qreal tooltipMargin() const;
    void setTooltipMargin(qreal margin);

    virtual bool eventFilter(QObject* target, QEvent* event) override;

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    void hideToolTip(bool animated = true);
    void showToolTip(bool animated = true);

    void setToolTipEnabled(bool enabled);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent* event) override;
    virtual void timerEvent(QTimerEvent* event) override;

    virtual QString toolTipAt(const QPointF& pos) const override;
    virtual bool showOwnTooltip(const QPointF& pos) override;

    virtual void setupShowAnimator(VariantAnimator* animator) const = 0;
    virtual void setupHideAnimator(VariantAnimator* animator) const = 0;

    /**
     * Check whether tooltip is actually visible (or animating right now) - or hidden (or hiding
     * right now)
     */
    bool actualToolTipVisibility() const;

    /**
     * Function to handle auto-hide behavior.
     */
    virtual bool calculateToolTipAutoVisibility() const;
    void updateToolTipAutoVisibility();

private:
    Q_SLOT void updateToolTipPosition();
    void updateToolTipOpacity();
    void updateToolTipText();

private:
    friend class QnToolTipSliderAnimationListener;
    friend class QnToolTipSliderVisibilityAccessor;

    QScopedPointer<QnToolTipSliderAnimationListener> m_animationListener;
    QPointer<QnToolTipWidget> m_tooltipWidget;
    VariantAnimator* m_tooltipWidgetVisibilityAnimator;
    qreal m_tooltipWidgetVisibility = 0.0;
    bool m_autoHideToolTip = true;
    bool m_toolTipAutoVisible = false; //< Last value of auto-hide calculation.
    int m_toolTipAutoHideTimerId = 0;
    QElapsedTimer m_lastSliderChangeTime;
    bool m_sliderUnderMouse = false;
    bool m_toolTipUnderMouse = false;
    bool m_pendingPositionUpdate = false;
    bool m_instantPositionUpdate = false;
    QPointF m_dragOffset;
    qreal m_tooltipMargin = 0.0;
    bool m_toolTipEnabled = true;
};
