// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QMargins>
#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/menu/action_target_provider.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/workbench/workbench_pane_settings.h>

class QGraphicsWidget;

class InstrumentManager;
class ActivityListenerInstrument;

class QnProxyLabel;
class QnDebugProxyLabel;
struct QnPaneSettings;

namespace nx::vms::client::desktop {

class AbstractWorkbenchPanel;
class CalendarWorkbenchPanel;
class DebugInfoInstrument;
class NotificationsWorkbenchPanel;
class PerformanceInfoWidget;
class TimelineWorkbenchPanel;
class TitleWorkbenchPanel;

namespace ui::workbench { class SpecialLayoutPanel; }

class WorkbenchUi:
    public QObject,
    public WindowContextAware,
    public menu::TargetProvider
{
    Q_OBJECT
    Q_ENUMS(Flags Flag)

    using base_type = QObject;
public:
    enum Flag
    {
        /** Whether controls should be hidden after a period without activity in zoomed mode. */
        HideWhenZoomed = 0x1,

        /** Whether controls should be hidden after a period without activity in normal mode. */
        HideWhenNormal = 0x2,

        /** Whether controls affect viewport margins. */
        AdjustMargins = 0x4
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Panel
    {
        NoPanel = 0x0,
        LeftPanel = 0x1,
        TitlePanel = 0x2,
        TimelinePanel = 0x4,
        NotificationsPanel = 0x8,
        LayoutPanel = 0x10
    };
    Q_DECLARE_FLAGS(Panels, Panel)

    WorkbenchUi(WindowContext* windowContext, QObject *parent = nullptr);
    virtual ~WorkbenchUi() override;

    virtual menu::ActionScope currentScope() const override;
    virtual menu::Parameters currentParameters(
        menu::ActionScope scope) const override;

    Flags flags() const;

    void setFlags(Flags flags);

    bool isPerformanceInfoVisible() const;

    /** Whether the Left-side Panel is opened */
    bool isLeftPanelOpened() const;

    /** Whether the Left-side Panel is pinned */
    bool isLeftPanelPinned() const;

    /** Whether navigation slider is opened. */
    bool isTimelineOpened() const;

    bool isTimelinePinned() const;

    /** Whether title bar is opened. */
    bool isTitleOpened() const;

    /** Whether notification pane is opened. */
    bool isNotificationsOpened() const;

    /** Whether notification pane is pinned. */
    bool isNotificationsPinned() const;

    /** Whether the calendar is pinned */
    bool isCalendarPinned() const;

    /** Whether the calendar is opened */
    bool isCalendarOpened() const;

    bool isLeftPanelVisible() const;
    bool isTimelineVisible() const;
    bool isTitleVisible() const;
    bool isNotificationsVisible() const;
    bool isCalendarVisible() const;

    void setTitleUsed(bool titleUsed = true);

private:
    void setPerformanceInfoVisible(bool performanceInfoVisible = true);
    void setDebugInfoVisible(bool debugInfoVisible = true);

    void setLeftPanelVisible(bool visible = true, bool animate = true);
    void setTimelineVisible(bool visible = true, bool animate = true);
    void setTitleVisible(bool visible = true, bool animate = true);
    void setNotificationsVisible(bool visible = true, bool animate = true);

    void setLeftPanelOpened(bool opened = true, bool animate = true);
    void setTimelineOpened(bool opened = true, bool animate = true);
    void setTitleOpened(bool opened = true, bool animate = true);
    void setNotificationsOpened(bool opened = true, bool animate = true);
    void setCalendarOpened(bool opened = true, bool animate = true);

private:
    void tick(int deltaMSecs);

    QMargins calculateViewportMargins(
        const QRectF& treeGeometry,
        const QRectF& titleGeometry,
        const QRectF& timelineGeometry,
        const QRectF& notificationsGeometry,
        const QRectF& layoutPanelGeometry);
    void updateViewportMargins(bool animate = true);
    void updateViewportMarginsAnimated();

    void updateLeftPanelGeometry();

    Q_SLOT void updateNotificationsGeometry();
    void updateFpsGeometry();
    void updateCalendarGeometry();
    void updateLayoutPanelGeometry();

    QRectF updatedLeftPanelGeometry(
        const QRectF& treeGeometry, const QRectF& titleGeometry, const QRectF& sliderGeometry);
    QRectF updatedNotificationsGeometry(
        const QRectF& notificationsGeometry, const QRectF& titleGeometry);
    void updateActivityInstrumentState();

    bool isHovered() const;
    void loadSettings(bool animated, bool useDefault);

    class StateDelegate;

    void createControlsWidget();
    void createFpsWidget();
    void createLeftPanelWidget(const QnPaneSettings& settings);
    void createTitleWidget(const QnPaneSettings& settings);
    void createLayoutPanelWidget(const QnPaneSettings& settings);
    void createNotificationsWidget(const QnPaneSettings& settings);
    void createCalendarWidget(const QnPaneSettings& settings);
    void createTimelineWidget(const QnPaneSettings& settings);
    void createDebugWidget();

    Panels openedPanels() const;
    void setOpenedPanels(Panels panels, bool animate = true);

    /** Make sure animation is allowed, set animate to false otherwise. */
    void ensureAnimationAllowed(bool &animate);

    void storeSettings();

    void updateCursor();

    bool calculateTimelineVisible(QnResourceWidget* widget) const;

    void updateCalendarVisibility(bool animate = true);
    void updateControlsVisibility(bool animate = true);

    void updateCalendarVisibilityAnimated();
    void updateControlsVisibilityAnimated();

    void updateLeftPanelMaximumWidth();

    void updateAutoFpsLimit();

    void at_freespaceAction_triggered();
    void at_fullscreenResourceAction_triggered();
    void at_activityStopped();
    void at_activityStarted();

    void at_display_widgetChanged(Qn::ItemRole role);

    void at_controlsWidget_geometryChanged();

private:
    /* Global state. */

    /** Instrument manager for the scene. */
    QPointer<InstrumentManager> m_instrumentManager;

    /** Activity listener instrument. */
    QPointer<ActivityListenerInstrument> m_controlsActivityInstrument;

    /** Current flags. */
    Flags m_flags;

    /** Widgets by role. */
    std::array<QPointer<QnResourceWidget>, Qn::ItemRoleCount> m_widgetByRole;

    /** Widget that ui controls are placed on. */
    QPointer<QGraphicsWidget> m_controlsWidget;

    /** Stored size of ui controls widget. */
    QRectF m_controlsWidgetRect;

    bool m_inactive = false;

    QPointer<PerformanceInfoWidget> m_performanceInfoWidget;
    QPointer<QnDebugProxyLabel> m_debugOverlayLabel;

    /* In freespace mode? */
    bool m_inFreespace = false;

    Panels m_unzoomedOpenedPanels;

    /* Timeline-related state. */

    /** Timeline. */
    QPointer<TimelineWorkbenchPanel> m_timeline;

    /* Left-side panel. */
    QPointer<AbstractWorkbenchPanel> m_leftPanel;

    /* Title-related state. */
    QPointer<TitleWorkbenchPanel> m_title;

    bool m_titleIsUsed = false;

    QPointer<ui::workbench::SpecialLayoutPanel> m_layoutPanel;

    /* Notifications window-related state. */
    QPointer<NotificationsWorkbenchPanel> m_notifications;

    /* Calendar window-related state. */
    QPointer<CalendarWorkbenchPanel> m_calendar;

    QnPaneSettingsMap m_settings;

    nx::utils::ScopedConnections m_connections;

    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WorkbenchUi::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(WorkbenchUi::Panels)

} // namespace nx::vms::client::desktop
