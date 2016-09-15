#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

class QnMaskedProxyWidget;
class QnCalendarWidget;
class QnImageButtonWidget;
class QnDayTimeWidget;
class VariantAnimator;
class AnimatorGroup;
class HoverFocusProcessor;

namespace NxUi {

class CalendarWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT
public:
    CalendarWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    QnMaskedProxyWidget* item;
    QnCalendarWidget* widget;
    QnImageButtonWidget* pinButton;
    QnImageButtonWidget* dayTimeMinimizeButton;
    VariantAnimator* yAnimator;

    bool inGeometryUpdate;
    bool inDayTimeGeometryUpdate;
    QPoint pinOffset;

    bool dayTimeOpened;
    QnMaskedProxyWidget* dayTimeItem;
    QnDayTimeWidget* dayTimeWidget;
    VariantAnimator* dayTimeSizeAnimator;

    QPoint dayTimeOffset;

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* hidingProcessor;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* opacityProcessor;

    /** Animator group for panel's opacity. */
    AnimatorGroup* opacityAnimatorGroup;


public:
    bool isEnabled() const;
    void setEnabled(bool enabled);

    QPointF origin() const;
    void setOrigin(const QPointF& position);

    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual QRectF effectiveGeometry() const override;

    void setDayTimeWidgetOpened(bool opened = true, bool animate = true);

protected:
    void setProxyUpdatesEnabled(bool updatesEnabled) override;

private:
    void updateControlsGeometry();
    void updateDayTimeWidgetGeometry();

private:
    void at_widget_dateClicked(const QDate &date);
    void at_dayTimeItem_paintGeometryChanged();

private:
    bool m_ignoreClickEvent;
    bool m_visible;

    QPointF m_origin;
};

} //namespace NxUi
