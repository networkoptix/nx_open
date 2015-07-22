#ifndef QN_WORKBENCH_UI_H
#define QN_WORKBENCH_UI_H

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

class QnNavigationItem;
class QnResourceBrowserWidget;
class QnProxyLabel;
class QnDebugProxyLabel;

class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnImageButtonWidget;
class QnResourceWidget;
class QnMaskedProxyWidget;
class QnAbstractRenderer;
class QnClickableWidget;
class QnFramedWidget;
class QnLayoutTabBar;
class QnActionManager;
class QnLayoutTabBar;
class QnGraphicsMessageBoxItem;
class QnNotificationsCollectionWidget;
class QnDayTimeWidget;

class QnSearchLineEdit;

class QnWorkbenchUi: public Disconnective<QObject>, public QnWorkbenchContextAware, public QnActionTargetProvider, public AnimationTimerListener, protected QnGeometry {
    Q_OBJECT
    Q_ENUMS(Flags Flag)

    typedef Disconnective<QObject> base_type;

public:
    enum Flag {
        /** Whether controls should be hidden after a period without activity in zoomed mode. */
        HideWhenZoomed = 0x1,

        /** Whether controls should be hidden after a period without activity in normal mode. */
        HideWhenNormal = 0x2,

        /** Whether controls affect viewport margins. */
        AdjustMargins = 0x4
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Panel {
        NoPanel = 0x0,
        TreePanel = 0x1,
        TitlePanel = 0x2,
        SliderPanel = 0x4,
        NotificationsPanel = 0x8
    };
    Q_DECLARE_FLAGS(Panels, Panel)

    QnWorkbenchUi(QObject *parent = NULL);

    virtual ~QnWorkbenchUi();

    virtual Qn::ActionScope currentScope() const override;
    virtual QnActionParameters currentParameters(Qn::ActionScope scope) const override;

    Flags flags() const {
        return m_flags;
    }

    void setFlags(Flags flags);

    bool isWindowButtonsUsed() const {
        return m_windowButtonsUsed;
    }

    bool isTitleUsed() const {
        return m_titleUsed;
    }

    bool isFpsVisible() const;

    /** Whether the tree is opened */
    bool isTreeOpened() const;

    /** Whether the tree is pinned */
    bool isTreePinned() const;

    /** Whether navigation slider is opened. */
    bool isSliderOpened() const;

    /** Whether title bar is opened. */
    bool isTitleOpened() const;

    bool isNotificationsOpened() const;

    /** Whether the calendar is pinned */
    bool isCalendarPinned() const;

    bool isCalendarOpened() const {
        return m_calendarOpened;
    }

    bool isTreeVisible() const {
        return m_treeVisible;
    }

    bool isSliderVisible() const {
        return m_sliderVisible;
    }

    bool isTitleVisible() const {
        return m_titleVisible;
    }

    bool isNotificationsVisible() const {
        return m_notificationsVisible;
    }

    bool isCalendarVisible() const {
        return m_calendarVisible;
    }

public slots:
    void setProxyUpdatesEnabled(bool updatesEnabled);
    void enableProxyUpdates() { setProxyUpdatesEnabled(true); }
    void disableProxyUpdates() { setProxyUpdatesEnabled(false); }

    void setTitleUsed(bool titleUsed = true);
    void setWindowButtonsUsed(bool windowButtonsUsed = true);
    void setFpsVisible(bool fpsVisible = true);

    void setTreeVisible(bool visible = true, bool animate = true);
    void setSliderVisible(bool visible = true, bool animate = true);
    void setTitleVisible(bool visible = true, bool animate = true);
    void setNotificationsVisible(bool visible = true, bool animate = true);
    void setCalendarVisible(bool visible = true, bool animate = true);
    void setSearchVisible(bool visible = true, bool animate = true);

    void setTreeOpened(bool opened = true, bool animate = true, bool save = true);
    void setSliderOpened(bool opened = true, bool animate = true, bool save = true);
    void setTitleOpened(bool opened = true, bool animate = true, bool save = true);
    void setNotificationsOpened(bool opened = true, bool animate = true, bool save = true);
    void setCalendarOpened(bool opened = true, bool animate = true);
    void setDayTimeWidgetOpened(bool opened = true, bool animate = true);
    void setSearchOpened(bool opened = true, bool animate = true);
protected:
    virtual bool event(QEvent *event) override;

    virtual void tick(int deltaMSecs) override;

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

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
    QRectF updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry, const QRectF &calendarGeometry, const QRectF &dayTimeGeometry, qreal *maxHeight);
    QRectF updatedCalendarGeometry(const QRectF &sliderGeometry);
    QRectF updatedDayTimeWidgetGeometry(const QRectF &sliderGeometry, const QRectF &calendarGeometry);
    void updateActivityInstrumentState();

    void setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setSliderOpacity(qreal opacity, bool animate);
    void setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setNotificationsOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setCalendarOpacity(qreal opacity, bool animate);
    void setSearchOpacity(qreal opacity, bool animate);
    void setSliderZoomButtonsOpacity(qreal opacity, bool animate);

    bool isThumbnailsVisible() const;
    void setThumbnailsVisible(bool visible);

    bool isHovered() const;

    void updateSearchGeometry();
    QRectF updatedSearchGeometry(const QRectF &sliderGeometry);

private:
    void createControlsWidget();
    void createFpsWidget();
    void createTreeWidget();
    void createTitleWidget();
    void createNotificationsWidget();
    void createCalendarWidget();
    void createSliderWidget();
    void createDebugWidget();
    void createSearchWidget();

    Panels openedPanels() const;
    void setOpenedPanels(Panels panels, bool animate = true, bool save = true);

    void initGraphicsMessageBox();

    /** Make sure animation is allowed, set animate to false otherwise. */
    void ensureAnimationAllowed(bool &animate);
private slots:
    void updateTreeOpacity(bool animate = true);
    void updateSliderOpacity(bool animate = true);
    void updateTitleOpacity(bool animate = true);
    void updateNotificationsOpacity(bool animate = true);
    void updateCalendarOpacity(bool animate = true);
    void updateSearchOpacity(bool animate = true);

    void updateCalendarVisibility(bool animate = true);
    void updateSearchVisibility(bool animate = true);
    void updateControlsVisibility(bool animate = true);
    
    void updateTreeOpacityAnimated() { updateTreeOpacity(true); }
    void updateSliderOpacityAnimated() { updateSliderOpacity(true); }
    void updateTitleOpacityAnimated() { updateTitleOpacity(true); }
    void updateNotificationsOpacityAnimated() { updateNotificationsOpacity(true); }
    void updateCalendarOpacityAnimated() { updateCalendarOpacity(true); }
    void updateCalendarVisibilityAnimated() { updateCalendarVisibility(true); }
    void updateSearchOpacityAnimated() { updateSearchOpacity(true); }
    void updateControlsVisibilityAnimated() { updateControlsVisibility(true); }

    void setTreeShowButtonUsed(bool used = true);
    void setNotificationsShowButtonUsed(bool used = true);

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
    void at_titleItem_contextMenuRequested(QObject *target, QEvent *event);

    void at_notificationsPinButton_toggled(bool checked);
    void at_notificationsShowingProcessor_hoverEntered();
    void at_notificationsItem_geometryChanged();

    void at_calendarItem_paintGeometryChanged();
    void at_dayTimeItem_paintGeometryChanged();

    void at_calendarWidget_dateClicked(const QDate &date);

    void at_searchItem_paintGeometryChanged();
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
    QnResourceWidget *m_widgetByRole[Qn::ItemRoleCount];

    /** Widget that ui controls are placed on. */
    QGraphicsWidget *m_controlsWidget;

    /** Stored size of ui controls widget. */
    QRectF m_controlsWidgetRect;

    bool m_treeVisible;

    bool m_titleUsed;

    bool m_titleVisible;

    bool m_sliderVisible;

    bool m_notificationsPinned;

    bool m_notificationsOpened;

    bool m_notificationsVisible;

    bool m_calendarOpened;

    bool m_calendarVisible;

    bool m_dayTimeOpened;

    bool m_windowButtonsUsed;

    bool m_searchVisible;

    bool m_searchOpened;

    bool m_ignoreClickEvent;

    bool m_inactive;

    QnProxyLabel *m_fpsItem;
    QnDebugProxyLabel* m_debugOverlayLabel;

    /* In freespace mode? */
    bool m_inFreespace;

    Panels m_unzoomedOpenedPanels;


    /* Slider-related state. */

    /** Navigation item. */
    QnNavigationItem *m_sliderItem;

    QGraphicsWidget *m_sliderResizerWidget;

    bool m_ignoreSliderResizerGeometryChanges;
    bool m_ignoreSliderResizerGeometryLater;

    bool m_ignoreTreeResizerGeometryChanges;
    bool m_updateTreeResizerGeometryLater;

    bool m_sliderZoomingIn, m_sliderZoomingOut;

    QGraphicsWidget *m_sliderZoomButtonsWidget;

    /** Hover processor that is used to change slider opacity when mouse is hovered over it. */
    HoverFocusProcessor *m_sliderOpacityProcessor;

    /** Animator for slider position. */
    VariantAnimator *m_sliderYAnimator;

    QnImageButtonWidget *m_sliderShowButton;

    AnimatorGroup *m_sliderOpacityAnimatorGroup;

    QTimer* m_sliderAutoHideTimer;

    qreal m_lastThumbnailsHeight;


    /* Tree-related state. */

    /** Navigation tree widget. */
    QnResourceBrowserWidget *m_treeWidget;
    QGraphicsWidget *m_treeResizerWidget;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget *m_treeItem;

    /** Item that provides background for the tree. */
    QnFramedWidget *m_treeBackgroundItem;

    /** Button to show/hide the tree. */
    QnImageButtonWidget *m_treeShowButton;

    /** Button to pin the tree. */
    QnImageButtonWidget *m_treePinButton;

    /** Hover processor that is used to hide the tree when the mouse leaves it. */
    HoverFocusProcessor *m_treeHidingProcessor;

    /** Hover processor that is used to show the tree when the mouse hovers over it. */
    HoverFocusProcessor *m_treeShowingProcessor;

    /** Hover processor that is used to change tree opacity when mouse hovers over it. */
    HoverFocusProcessor *m_treeOpacityProcessor;

    /** Animator group for tree's opacity. */
    AnimatorGroup *m_treeOpacityAnimatorGroup;

    /** Animator for tree's position. */
    VariantAnimator *m_treeXAnimator;



    /* Title-related state. */

    /** Title bar widget. */
    QnClickableWidget *m_titleItem;

    QnImageButtonWidget *m_titleShowButton;

    QnImageButtonWidget *m_mainMenuButton;

    QnLayoutTabBar *m_tabBarWidget;

    QGraphicsProxyWidget *m_tabBarItem;

    AnimatorGroup *m_titleOpacityAnimatorGroup;

    /** Background widget for the title bar. */
    QnFramedWidget *m_titleBackgroundItem;

    /** Animator for title's position. */
    VariantAnimator *m_titleYAnimator;

    HoverFocusProcessor *m_titleOpacityProcessor;

    QGraphicsLinearLayout *m_titleRightButtonsLayout;

    QGraphicsWidget *m_windowButtonsWidget;


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

    bool m_inCalendarGeometryUpdate;

    bool m_inDayTimeGeometryUpdate;

    QnMaskedProxyWidget *m_dayTimeItem;

    QnDayTimeWidget *m_dayTimeWidget;

    VariantAnimator *m_dayTimeSizeAnimator;

    /* Search widget */
    QnMaskedProxyWidget *m_searchWidget;
    VariantAnimator *m_searchSizeAnimator;
    AnimatorGroup *m_searchOpacityAnimatorGroup;
    HoverFocusProcessor *m_searchOpacityProcessor;

    bool m_inSearchGeometryUpdate;

    qreal m_pinOffset;
    QPoint m_calendarPinOffset;
    QPoint m_dayTimeOffset;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Panels)

#endif // QN_WORKBENCH_UI_H
