// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_workbench_panel.h"

class ActivityListenerInstrument;
class QnNavigationItem;
class QnResizerWidget;
class QGraphicsWidget;
class HoverFocusProcessor;
class VariantAnimator;
class QnBlinkingImageButtonWidget;
class QnImageButtonWidget;
class AnimatorGroup;
class QTimer;

namespace nx::vms::client::desktop {

namespace workbench::timeline {

class NavigationWidget;
class ControlWidget;

} // workbench::timelines

class CalendarWorkbenchPanel;

class TimelineWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT
public:
    TimelineWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    virtual ~TimelineWorkbenchPanel();

    QnNavigationItem* item;

    void setCalendarPanel(CalendarWorkbenchPanel* calendar);

public:
    virtual bool isPinned() const override;
    bool isPinnedManually() const;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual QRectF effectiveGeometry() const override;
    virtual QRectF geometry() const override;

    virtual void stopAnimations() override;

    bool isThumbnailsVisible() const;
    void setThumbnailsVisible(bool visible);

    // Used for panel state restore.
    void setThumbnailsState(bool visible, qreal lastThumbnailsHeight);
    qreal thumbnailsHeight() const;

    virtual void setPanelSize(qreal size) override;
    void updateGeometry();

private:
    void setShowButtonUsed(bool used);
    void setShowButtonVisible(bool visible);
    void updateResizerGeometry();
    void updateControlsGeometry();
    void updateTooltipVisibility();
    void updateShowButtonVisibility();

    qreal minimumHeight() const;

private:
    void at_resizerWidget_geometryChanged();
    void at_sliderResizerWidget_wheelEvent(QObject* target, QEvent* event);

private:
    qreal m_lastThumbnailsHeight;

    bool m_ignoreClickEvent;
    bool m_visible;
    bool m_isPinnedManually;

    /** We are currently in the resize process. */
    bool m_resizing;

    bool m_updateResizerGeometryLater;

    int m_autoHideHeight;

    workbench::timeline::NavigationWidget* m_navigationWidget;
    workbench::timeline::ControlWidget* m_controlWidget;

    QPointer<CalendarWorkbenchPanel> m_calendar;

    QnImageButtonWidget* m_pinButton;
    QnBlinkingImageButtonWidget* m_showButton;

    QnResizerWidget* m_resizerWidget;

    /** Special widget to show by hover. */
    QGraphicsWidget* m_showWidget;

    QTimer* m_autoHideTimer;

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* m_hidingProcessor;

    /** Hover processor that is used to show the panel when the mouse hovers over show button. */
    HoverFocusProcessor* m_showingProcessor;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;

    /** Animator for y position. */
    VariantAnimator* m_yAnimator;

    ActivityListenerInstrument* m_widgetActivityInstrument;
};

} //namespace nx::vms::client::desktop
