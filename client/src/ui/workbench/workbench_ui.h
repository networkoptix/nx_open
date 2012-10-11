#ifndef QN_WORKBENCH_UI_H
#define QN_WORKBENCH_UI_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/common/geometry.h>
#include <ui/actions/action_target_provider.h>

#include "workbench_globals.h"
#include "workbench_context_aware.h"

class QGraphicsProxyWidget;
class QGraphicsWidget;
class QGraphicsLinearLayout;

class QnVideoCamera;

class InstrumentManager;
class UiElementsInstrument;
class ActivityListenerInstrument;
class FpsCountingInstrument;
class VariantAnimator;
class AnimatorGroup;
class HoverFocusProcessor;

class QnNavigationItem;
class QnResourceTreeWidget;
class GraphicsLabel;

class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnImageButtonWidget;
class QnResourceWidget;
class QnMaskedProxyWidget;
class QnAbstractRenderer;
class QnClickableWidget;
class QnSimpleFrameWidget;
class QnLayoutTabBar;
class QnHelpWidget;
class QnActionManager;
class QnLayoutTabBar;
class QnWorkbenchMotionDisplayWatcher;

class QnWorkbenchUi: public QObject, public QnWorkbenchContextAware, public QnActionTargetProvider, protected QnGeometry {
    Q_OBJECT;
    Q_ENUMS(Flags Flag);

    typedef QObject base_type;

public:
    enum Flag {
        /** Whether controls should be hidden after a period without activity in zoomed mode. */
        HideWhenZoomed = 0x1, 

        /** Whether controls should be hidden after a period without activity in normal mode. */
        HideWhenNormal = 0x2, 

        /** Whether controls affect viewport margins. */
        AdjustMargins = 0x4,
    };
    Q_DECLARE_FLAGS(Flags, Flag);

    enum Panel {
        NoPanel = 0x0,
        TreePanel = 0x1,
        TitlePanel = 0x2,
        SliderPanel = 0x4,
        HelpPanel = 0x8
    };
    Q_DECLARE_FLAGS(Panels, Panel);

    QnWorkbenchUi(QObject *parent = NULL);

    virtual ~QnWorkbenchUi();

    virtual Qn::ActionScope currentScope() const override;

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

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

    bool isTreeOpened() const {
        return m_treeOpened;
    }

    bool isSliderOpened() const {
        return m_sliderOpened;
    }

    bool isTitleOpened() const {
        return m_titleUsed && m_titleOpened;
    }

    bool isHelpOpened() const {
        return m_helpOpened;
    }

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

    bool isHelpVisible() const {
        return m_helpVisible;
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
    void setHelpVisible(bool visible = true, bool animate = true);
    void setCalendarVisible(bool visible = true, bool animate = true);

    void setTreeOpened(bool opened = true, bool animate = true);
    void setSliderOpened(bool opened = true, bool animate = true);
    void setTitleOpened(bool opened = true, bool animate = true);
    void setHelpOpened(bool opened = true, bool animate = true);
    void setCalendarOpened(bool opened = true, bool animate = true);

    void toggleTreeOpened() {
        setTreeOpened(!isTreeOpened());
    }

    void toggleSliderOpened() {
        setSliderOpened(!isSliderOpened());
    }

    void toggleTitleOpened() {
        setTitleOpened(!isTitleOpened());
    }

    void toggleHelpOpened() {
        setHelpOpened(!isHelpOpened());
    }

protected:
    virtual bool event(QEvent *event) override;

    QMargins calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal helpY);
    void updateViewportMargins();

    void updateTreeGeometry();
    void updateHelpGeometry();
    void updateFpsGeometry();
    void updateCalendarGeometry();
    Q_SLOT void updateSliderResizerGeometry();
    void updatePanicButtonGeometry();

    QRectF updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);
    QRectF updatedHelpGeometry(const QRectF &helpGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry, const QRectF &calendarGeometry);
    QRectF updatedCalendarGeometry(const QRectF &sliderGeometry);
    void updateActivityInstrumentState();

    void setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setSliderOpacity(qreal opacity, bool animate);
    void setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setHelpOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setCalendarOpacity(qreal opacity, bool animate);

    bool isThumbnailsVisible() const;
    void setThumbnailsVisible(bool visible);

private:
    Panels openedPanels() const;
    void setOpenedPanels(Panels panels);

private slots:
    void updateHelpContext();
    
    void updateTreeOpacity(bool animate = true);
    void updateSliderOpacity(bool animate = true);
    void updateTitleOpacity(bool animate = true);
    void updateHelpOpacity(bool animate = true);
    void updateCalendarOpacity(bool animate = true);
    void updateCalendarVisibility(bool animate = true);
    void updateControlsVisibility(bool animate = true);

    void setTreeShowButtonUsed(bool used = true);
    void setHelpShowButtonUsed(bool used = true);

    void at_freespaceAction_triggered();
    void at_fullscreenAction_triggered();
    void at_activityStopped();
    void at_activityStarted();
    void at_fpsChanged(qreal fps);

    void at_display_widgetChanged(Qn::ItemRole role);

    void at_controlsWidget_deactivated();
    void at_controlsWidget_geometryChanged();

    void at_sliderItem_geometryChanged();
    void at_sliderResizerItem_geometryChanged();
    void at_sliderShowButton_toggled(bool checked);
    void at_toggleThumbnailsAction_toggled(bool checked);
    void at_toggleCalendarAction_toggled(bool checked);
    void at_toggleSliderAction_toggled(bool checked);
    
    void at_treeWidget_activated(const QnResourcePtr &resource);
    void at_treeItem_paintGeometryChanged();
    void at_treeHidingProcessor_hoverFocusLeft();
    void at_treeShowingProcessor_hoverEntered();
    void at_treeShowButton_toggled(bool checked);
    void at_treePinButton_toggled(bool checked);
    void at_toggleTreeAction_toggled(bool checked);

    void at_tabBar_closeRequested(QnWorkbenchLayout *layout);
    void at_titleItem_geometryChanged();
    void at_titleItem_contextMenuRequested(QObject *target, QEvent *event);
    void at_titleShowButton_toggled(bool checked);
    void at_toggleTitleBarAction_toggled(bool checked);

    void at_helpPinButton_toggled(bool checked);
    void at_helpShowButton_toggled(bool checked);
    void at_helpHidingProcessor_hoverFocusLeft();
    void at_helpShowingProcessor_hoverEntered();
    void at_helpItem_paintGeometryChanged();
    void at_helpWidget_showRequested();
    void at_helpWidget_hideRequested();

    void at_calendarShowButton_toggled(bool checked);
    void at_calendarItem_paintGeometryChanged();

    void at_fpsItem_geometryChanged();

private:
    /* Global state. */

    /** Instrument manager for the scene. */
    InstrumentManager *m_instrumentManager;

    /** Ui elements instrument. */
    UiElementsInstrument *m_uiElementsInstrument;

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

    /** Whether the tree is pinned. */
    bool m_treePinned;

    /** Whether the tree is opened. */
    bool m_treeOpened;

    bool m_treeVisible;

    bool m_titleUsed;

    /** Whether title bar is opened. */
    bool m_titleOpened;

    bool m_titleVisible;

    /** Whether navigation slider is opened. */
    bool m_sliderOpened;

    bool m_sliderVisible;

    bool m_helpPinned;

    bool m_helpOpened;

    bool m_helpVisible;

    bool m_calendarOpened;

    bool m_calendarVisible;

    bool m_windowButtonsUsed;

    bool m_ignoreClickEvent;

    bool m_inactive;

    GraphicsLabel *m_fpsItem;

    /* In freespace mode? */
    bool m_inFreespace;

    Panels m_unzoomedOpenedPanels;


    /* Slider-related state. */

    /** Navigation item. */
    QnNavigationItem *m_sliderItem;

    QGraphicsWidget *m_sliderResizerItem;

    QGraphicsWidget *m_sliderEaterItem;

    bool m_ignoreSliderResizerGeometryChanges;
    bool m_ignoreSliderResizerGeometryChanges2;

    /** Hover processor that is used to change slider opacity when mouse is hovered over it. */
    HoverFocusProcessor *m_sliderOpacityProcessor;

    /** Animator for slider position. */
    VariantAnimator *m_sliderYAnimator;

    QnImageButtonWidget *m_sliderShowButton;

    AnimatorGroup *m_sliderOpacityAnimatorGroup;

    qreal m_lastThumbnailsHeight;


    /* Tree-related state. */

    /** Navigation tree widget. */
    QnResourceTreeWidget *m_treeWidget;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget *m_treeItem;

    /** Item that provides background for the tree. */
    QnSimpleFrameWidget *m_treeBackgroundItem;

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
    QnSimpleFrameWidget *m_titleBackgroundItem;

    /** Animator for title's position. */
    VariantAnimator *m_titleYAnimator;

    HoverFocusProcessor *m_titleOpacityProcessor;

    QGraphicsLinearLayout *m_titleRightButtonsLayout;

    QGraphicsWidget *m_windowButtonsWidget;


    /* Help window-related state. */

    QnSimpleFrameWidget *m_helpBackgroundItem;

    QnMaskedProxyWidget *m_helpItem;

    QnHelpWidget *m_helpWidget;

    QnImageButtonWidget *m_helpPinButton;

    QnImageButtonWidget *m_helpShowButton;

    HoverFocusProcessor *m_helpOpacityProcessor;

    HoverFocusProcessor *m_helpHidingProcessor;

    HoverFocusProcessor *m_helpShowingProcessor;

    VariantAnimator *m_helpXAnimator;

    AnimatorGroup *m_helpOpacityAnimatorGroup;

    QnWorkbenchMotionDisplayWatcher *m_motionDisplayWatcher;


    /* Calendar window-related state. */

    QnMaskedProxyWidget *m_calendarItem;

    VariantAnimator *m_calendarSizeAnimator;

    QnImageButtonWidget *m_calendarShowButton;

    AnimatorGroup *m_calendarOpacityAnimatorGroup;

    HoverFocusProcessor *m_calendarOpacityProcessor;

    bool m_inCalendarGeometryUpdate;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Flags);
Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Panels);

#endif // QN_WORKBENCH_UI_H
