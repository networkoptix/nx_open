#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <utils/common/disconnective.h>

#include <core/resource/resource_fwd.h>

#include <ui/common/geometry.h>
#include <ui/actions/action_target_provider.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/workbench/ui/timeline.h>
#include <ui/workbench/ui/resource_tree.h>

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
class QnNotificationsCollectionWidget;
class QnDayTimeWidget;
struct QnPaneSettings;

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
        SliderPanel = 0x4,
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
    bool isSliderOpened() const;

    bool isSliderPinned() const;

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
    bool isSliderVisible() const;
    bool isTitleVisible() const;
    bool isNotificationsVisible() const;
    bool isCalendarVisible() const;

public slots:
    void setProxyUpdatesEnabled(bool updatesEnabled);
    void enableProxyUpdates();
    void disableProxyUpdates();

    void setTitleUsed(bool titleUsed = true);
    void setFpsVisible(bool fpsVisible = true);

    void setTreeVisible(bool visible = true, bool animate = true);
    void setSliderVisible(bool visible = true, bool animate = true);
    void setTitleVisible(bool visible = true, bool animate = true);
    void setNotificationsVisible(bool visible = true, bool animate = true);
    void setCalendarVisible(bool visible = true, bool animate = true);

    void setTreeOpened(bool opened = true, bool animate = true);
    void setSliderOpened(bool opened = true, bool animate = true);
    void setTitleOpened(bool opened = true, bool animate = true);
    void setNotificationsOpened(bool opened = true, bool animate = true);
    void setCalendarOpened(bool opened = true, bool animate = true);
    void setDayTimeWidgetOpened(bool opened = true, bool animate = true);

protected:
    virtual void tick(int deltaMSecs) override;

    QMargins calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal notificationsX);
    void updateViewportMargins();

    void updateTreeGeometry();
    Q_SLOT void updateTreeResizerGeometry();
    Q_SLOT void updateNotificationsGeometry();
    void updateFpsGeometry();
    void updateCalendarGeometry();
    void updateDayTimeWidgetGeometry();
    Q_SLOT void updateSliderResizerGeometry();
    void updateSliderZoomButtonsGeometry();

    QRectF updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);
    QRectF updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);
    QRectF updatedCalendarGeometry(const QRectF &sliderGeometry);
    QRectF updatedDayTimeWidgetGeometry(const QRectF &sliderGeometry, const QRectF &calendarGeometry);
    void updateActivityInstrumentState();

    void setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setSliderOpacity(qreal opacity, bool animate);
    void setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setNotificationsOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setCalendarOpacity(qreal opacity, bool animate);
    void setSliderZoomButtonsOpacity(qreal opacity, bool animate);

    bool isThumbnailsVisible() const;
    void setThumbnailsVisible(bool visible);

    bool isHovered() const;

private:
    void createControlsWidget();
    void createFpsWidget();
    void createTreeWidget(const QnPaneSettings& settings);
    void createTitleWidget(const QnPaneSettings& settings);
    void createNotificationsWidget(const QnPaneSettings& settings);
    void createCalendarWidget(const QnPaneSettings& settings);
    void createSliderWidget(const QnPaneSettings& settings);

#ifdef _DEBUG
    void createDebugWidget();
#endif

    Panels openedPanels() const;
    void setOpenedPanels(Panels panels, bool animate = true);

    void initGraphicsMessageBox();

    /** Make sure animation is allowed, set animate to false otherwise. */
    void ensureAnimationAllowed(bool &animate);

    void storeSettings();

    void updateCursor();

    QnImageButtonWidget* newActionButton(QGraphicsItem *parent, QAction* action, int helpTopicId);
    QnImageButtonWidget* newShowHideButton(QGraphicsItem* parent, QAction* action);
    QnImageButtonWidget* newPinButton(QGraphicsItem* parent, QAction* action, bool smallIcon = false);

private slots:
    void updateTreeOpacity(bool animate = true);
    void updateSliderOpacity(bool animate = true);
    void updateTitleOpacity(bool animate = true);
    void updateNotificationsOpacity(bool animate = true);
    void updateCalendarOpacity(bool animate = true);

    void updateCalendarVisibility(bool animate = true);
    void updateControlsVisibility(bool animate = true);

    void updateTreeOpacityAnimated() { updateTreeOpacity(true); }
    void updateSliderOpacityAnimated() { updateSliderOpacity(true); }
    void updateTitleOpacityAnimated() { updateTitleOpacity(true); }
    void updateNotificationsOpacityAnimated() { updateNotificationsOpacity(true); }
    void updateCalendarOpacityAnimated() { updateCalendarOpacity(true); }
    void updateCalendarVisibilityAnimated() { updateCalendarVisibility(true); }
    void updateControlsVisibilityAnimated() { updateControlsVisibility(true); }

    void setSliderShowButtonUsed(bool used);
    void setTreeShowButtonUsed(bool used);
    void setNotificationsShowButtonUsed(bool used);
    void setCalendarShowButtonUsed(bool used);

    void at_freespaceAction_triggered();
    void at_activityStopped();
    void at_activityStarted();

    void at_display_widgetChanged(Qn::ItemRole role);

    void at_controlsWidget_geometryChanged();

    void at_sliderResizerWidget_wheelEvent(QObject *target, QEvent *event);
    void at_sliderItem_geometryChanged();
    void at_sliderResizerWidget_geometryChanged();

    void at_treeWidget_activated(const QnResourcePtr &resource);
    void at_treeItem_paintGeometryChanged();
    void at_treeResizerWidget_geometryChanged();
    void at_treeShowingProcessor_hoverEntered();
    void at_pinTreeAction_toggled(bool checked);
    void at_pinNotificationsAction_toggled(bool checked);

    void at_titleItem_geometryChanged();

    void at_notificationsShowingProcessor_hoverEntered();
    void at_notificationsItem_geometryChanged();

    void at_calendarShowingProcessor_hoverEntered();
    void at_calendarItem_paintGeometryChanged();
    void at_dayTimeItem_paintGeometryChanged();

    void at_calendarWidget_dateClicked(const QDate &date);

private:
    /* Global state. */

    /** Instrument manager for the scene. */
    InstrumentManager *m_instrumentManager;

    /** Fps counting instrument. */
    FpsCountingInstrument *m_fpsCountingInstrument;

    /** Activity listener instrument. */
    ActivityListenerInstrument *m_controlsActivityInstrument;

    /** Current flags. */
    Flags m_flags;

    /** Widgets by role. */
    std::array<QnResourceWidget*, Qn::ItemRoleCount> m_widgetByRole;

    /** Widget that ui controls are placed on. */
    QGraphicsWidget *m_controlsWidget;

    /** Stored size of ui controls widget. */
    QRectF m_controlsWidgetRect;

    bool m_treeVisible;

    bool m_titleUsed;

    bool m_titleVisible;

    bool m_notificationsVisible;

    bool m_calendarVisible;

    bool m_dayTimeOpened;

    bool m_ignoreClickEvent;

    bool m_inactive;

    QnProxyLabel *m_fpsItem;
    QnDebugProxyLabel* m_debugOverlayLabel;

    /* In freespace mode? */
    bool m_inFreespace;

    Panels m_unzoomedOpenedPanels;

    /* Slider-related state. */

    /** Navigation item. */
    QnWorkbenchUiTimeline m_timeline;

    /* Resources tree. */
    QnWorkbenchUiResourceTree m_tree;

    /* Title-related state. */

    /** Title bar widget. */
    QGraphicsProxyWidget *m_titleItem;

    QnImageButtonWidget *m_titleShowButton;

    AnimatorGroup *m_titleOpacityAnimatorGroup;

    /** Animator for title's position. */
    VariantAnimator *m_titleYAnimator;

    HoverFocusProcessor *m_titleOpacityProcessor;

    /* Notifications window-related state. */

    QnFramedWidget *m_notificationsBackgroundItem;

    QnNotificationsCollectionWidget *m_notificationsItem;

    QnImageButtonWidget *m_notificationsPinButton;

    QnImageButtonWidget *m_notificationsShowButton;

    HoverFocusProcessor *m_notificationsOpacityProcessor;

    HoverFocusProcessor *m_notificationsHidingProcessor;

    HoverFocusProcessor *m_notificationsShowingProcessor;

    VariantAnimator *m_notificationsXAnimator;

    AnimatorGroup *m_notificationsOpacityAnimatorGroup;


    /* Calendar window-related state. */

    QnMaskedProxyWidget *m_calendarItem;

    QnImageButtonWidget *m_calendarPinButton;

    QnImageButtonWidget *m_dayTimeMinimizeButton;

    VariantAnimator *m_calendarSizeAnimator;

    AnimatorGroup *m_calendarOpacityAnimatorGroup;

    HoverFocusProcessor *m_calendarOpacityProcessor;

    HoverFocusProcessor *m_calendarHidingProcessor;

    HoverFocusProcessor *m_calendarShowingProcessor;

    bool m_inCalendarGeometryUpdate;

    bool m_inDayTimeGeometryUpdate;

    QnMaskedProxyWidget *m_dayTimeItem;

    QnDayTimeWidget *m_dayTimeWidget;

    VariantAnimator *m_dayTimeSizeAnimator;

    QPoint m_calendarPinOffset;
    QPoint m_dayTimeOffset;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Panels)
