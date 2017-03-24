#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

class QGraphicsItem;
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

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* hidingProcessor;

public:
    bool isEnabled() const;
    void setEnabled(bool enabled, bool animated);

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

    virtual void stopAnimations() override;

    void setDayTimeWidgetOpened(bool opened = true, bool animate = true);

    //TODO: #gdm Probably make this AbstractWorkbenchPanel interface function
    QList<QGraphicsItem*> activeItems() const;

protected:
    void setProxyUpdatesEnabled(bool updatesEnabled) override;

private:
    void updateControlsGeometry();

private:
    void at_widget_dateClicked(const QDate &date);

private:
    bool m_ignoreClickEvent;
    bool m_visible;

    QPointF m_origin;

    QnCalendarWidget* m_widget;
    QnImageButtonWidget* m_pinButton;
    QnImageButtonWidget* m_dayTimeMinimizeButton;
    VariantAnimator* m_yAnimator;

    QPoint m_pinOffset;

    bool m_dayTimeOpened;
    QnMaskedProxyWidget* m_dayTimeItem;
    QnDayTimeWidget* m_dayTimeWidget;

    QPoint m_dayTimeOffset;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;
};

} //namespace NxUi
