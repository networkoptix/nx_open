#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <utils/common/disconnective.h>

#include <core/resource/resource_fwd.h>

#include <ui/common/geometry.h>
#include <ui/actions/action_target_provider.h>
#include <ui/animation/animation_timer_listener.h>

#include <client/client_globals.h>

#include "workbench_context_aware.h"

class QGraphicsProxyWidget;
class QGraphicsWidget;
class QGraphicsLinearLayout;

class InstrumentManager;
class ActivityListenerInstrument;
class FpsCountingInstrument;
class VariantAnimator;
class AnimatorGroup;
class HoverFocusProcessor;

class QnProxyLabel;
class QnDebugProxyLabel;

class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnImageButtonWidget;
class QnResourceWidget;
class QnMaskedProxyWidget;
class QnFramedWidget;
class QnDayTimeWidget;
struct QnPaneSettings;

namespace NxUi {

class ResourceTreeWorkbenchPanel;
class NotificationsWorkbenchPanel;
class TimelineWorkbenchPanel;
class CalendarWorkbenchPanel;

}

class QnWorkbenchUi:
    public Disconnective<QObject>,
    public QnWorkbenchContextAware,
    public QnActionTargetProvider,
    public AnimationTimerListener,
    protected QnGeometry
{
    Q_OBJECT
    Q_ENUMS(Flags Flag)

    using base_type = Disconnective<QObject>;
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
        TreePanel = 0x1,
        TitlePanel = 0x2,
        TimelinePanel = 0x4,
        NotificationsPanel = 0x8
    };
    Q_DECLARE_FLAGS(Panels, Panel)

    QnWorkbenchUi(QObject *parent = nullptr);

    virtual ~QnWorkbenchUi();

    virtual Qn::ActionScope currentScope() const override;
    virtual QnActionParameters currentParameters(Qn::ActionScope scope) const override;

    Flags flags() const;

    void setFlags(Flags flags);

    bool isTitleUsed() const;

    bool isFpsVisible() const;

    /** Whether the tree is opened */
    bool isTreeOpened() const;

    /** Whether the tree is pinned */
    bool isTreePinned() const;

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

    bool isTreeVisible() const;
    bool isTimelineVisible() const;
    bool isTitleVisible() const;
    bool isNotificationsVisible() const;
    bool isCalendarVisible() const;

public slots:
    void setTitleUsed(bool titleUsed = true);
    void setFpsVisible(bool fpsVisible = true);

    void setTreeVisible(bool visible = true, bool animate = true);
    void setTimelineVisible(bool visible = true, bool animate = true);
    void setTitleVisible(bool visible = true, bool animate = true);
    void setNotificationsVisible(bool visible = true, bool animate = true);

    void setTreeOpened(bool opened = true, bool animate = true);
    void setTimelineOpened(bool opened = true, bool animate = true);
    void setTitleOpened(bool opened = true, bool animate = true);
    void setNotificationsOpened(bool opened = true, bool animate = true);
    void setCalendarOpened(bool opened = true, bool animate = true);


protected:
    virtual void tick(int deltaMSecs) override;

    QMargins calculateViewportMargins(
        QRectF treeGeometry,
        qreal titleY, qreal titleH,
        QRectF timelineGeometry,
        QRectF notificationsGeometry);
    void updateViewportMargins();

    void updateTreeGeometry();

    Q_SLOT void updateNotificationsGeometry();
    void updateFpsGeometry();
    void updateCalendarGeometry();

    QRectF updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);
    QRectF updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry);
    void updateActivityInstrumentState();

    void setTitleOpacity(qreal opacity, bool animate);

    bool isHovered() const;

private:
    void createControlsWidget();
    void createFpsWidget();
    void createTreeWidget(const QnPaneSettings& settings);
    void createTitleWidget(const QnPaneSettings& settings);
    void createNotificationsWidget(const QnPaneSettings& settings);
    void createCalendarWidget(const QnPaneSettings& settings);
    void createTimelineWidget(const QnPaneSettings& settings);

#ifdef _DEBUG
    void createDebugWidget();
#endif

    Panels openedPanels() const;
    void setOpenedPanels(Panels panels, bool animate = true);

    void initGraphicsMessageBoxHolder();

    /** Make sure animation is allowed, set animate to false otherwise. */
    void ensureAnimationAllowed(bool &animate);

    void storeSettings();

    void updateCursor();

    bool calculateTimelineVisible(QnResourceWidget* widget) const;

private slots:
    void updateTitleOpacity(bool animate = true);

    void updateCalendarVisibility(bool animate = true);
    void updateControlsVisibility(bool animate = true);

    void updateTitleOpacityAnimated();
    void updateCalendarVisibilityAnimated();
    void updateControlsVisibilityAnimated();

    void at_freespaceAction_triggered();
    void at_activityStopped();
    void at_activityStarted();

    void at_display_widgetChanged(Qn::ItemRole role);

    void at_controlsWidget_geometryChanged();

    void at_titleItem_geometryChanged();

private:
    /* Global state. */

    /** Instrument manager for the scene. */
    QPointer<InstrumentManager> m_instrumentManager;

    /** Fps counting instrument. */
    QPointer<FpsCountingInstrument> m_fpsCountingInstrument;

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

    bool m_titleUsed;

    bool m_titleVisible;

    bool m_ignoreClickEvent;

    bool m_inactive;

    QPointer<QnProxyLabel> m_fpsItem;
    QPointer<QnDebugProxyLabel> m_debugOverlayLabel;

    /* In freespace mode? */
    bool m_inFreespace;

    Panels m_unzoomedOpenedPanels;

    /* Timeline-related state. */

    /** Timeline. */
    QPointer<NxUi::TimelineWorkbenchPanel> m_timeline;

    /* Resources tree. */
    QPointer<NxUi::ResourceTreeWorkbenchPanel> m_tree;

    /* Title-related state. */

    /** Title bar widget. */
    QPointer<QGraphicsProxyWidget> m_titleItem;

    QPointer<QnImageButtonWidget> m_titleShowButton;

    QPointer<AnimatorGroup> m_titleOpacityAnimatorGroup;

    /** Animator for title's position. */
    QPointer<VariantAnimator> m_titleYAnimator;

    QPointer<HoverFocusProcessor> m_titleOpacityProcessor;

    /* Notifications window-related state. */
    QPointer<NxUi::NotificationsWorkbenchPanel> m_notifications;

    /* Calendar window-related state. */
    QPointer<NxUi::CalendarWorkbenchPanel> m_calendar;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Panels)
