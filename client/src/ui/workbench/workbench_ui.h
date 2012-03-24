#ifndef QN_WORKBENCH_UI_H
#define QN_WORKBENCH_UI_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <ui/common/scene_utility.h>
#include <ui/actions/action_target_provider.h>
#include "workbench.h" /* For QnWorkbench::ItemRole. */
#include "workbench_context_aware.h"

class QGraphicsProxyWidget;

class CLVideoCamera;

class InstrumentManager;
class UiElementsInstrument;
class ActivityListenerInstrument;
class FpsCountingInstrument;
class VariantAnimator;
class AnimatorGroup;
class HoverFocusProcessor;

class NavigationItem;
class QnResourceTreeWidget;
class GraphicsLabel;

class QnWorkbenchDisplay;
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

class QnWorkbenchUi: public QObject, public QnWorkbenchContextAware, public QnActionTargetProvider, protected SceneUtility {
    Q_OBJECT;
    Q_ENUMS(Flags Flag);

public:
    enum Flag {
        HIDE_WHEN_ZOOMED = 0x1, /**< Whether controls should be hidden after a period without activity in zoomed mode. */
        HIDE_WHEN_NORMAL = 0x2, /**< Whether controls should be hidden after a period without activity in normal mode. */
        AFFECT_MARGINS_WHEN_ZOOMED = 0x4,
        AFFECT_MARGINS_WHEN_NORMAL = 0x8
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QnWorkbenchUi(QnWorkbenchDisplay *display, QObject *parent = NULL);

    virtual ~QnWorkbenchUi();

    virtual Qn::ActionScope currentScope() const override;

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    QnWorkbenchDisplay *display() const;

    Flags flags() const {
        return m_flags;
    }

    void setFlags(Flags flags);

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

public slots:
    void setProxyUpdatesEnabled(bool updatesEnabled);
    void enableProxyUpdates() { setProxyUpdatesEnabled(true); }
    void disableProxyUpdates() { setProxyUpdatesEnabled(false); }

    void setTitleUsed(bool titleUsed = true);
    void setFpsVisible(bool fpsVisible = true);

    void setTreeVisible(bool visible = true, bool animate = true);
    void setSliderVisible(bool visible = true, bool animate = true);
    void setTitleVisible(bool visible = true, bool animate = true);
    void setHelpVisible(bool visible = true, bool animate = true);

    void setTreeOpened(bool opened = true, bool animate = true);
    void setSliderOpened(bool opened = true, bool animate = true);
    void setTitleOpened(bool opened = true, bool animate = true);
    void setHelpOpened(bool opened = true, bool animate = true);

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
    QMargins calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal helpY);
    void updateViewportMargins();

    void updateTreeGeometry();
    void updateHelpGeometry();
    void updateFpsGeometry();

    QRectF updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);
    QRectF updatedHelpGeometry(const QRectF &helpGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);
    void updateActivityInstrumentState();

    void setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setSliderOpacity(qreal opacity, bool animate);
    void setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);
    void setHelpOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate);

protected slots:
    void updateHelpContext();
    void updateHelpContextInternal();
    
    void updateTreeOpacity(bool animate = true);
    void updateSliderOpacity(bool animate = true);
    void updateTitleOpacity(bool animate = true);
    void updateHelpOpacity(bool animate = true);
    void updateControlsVisibility(bool animate = true);

    void setTreeShowButtonUsed(bool used = true);
    void setHelpShowButtonUsed(bool used = true);

    void at_mainMenuAction_triggered();
    void at_freespaceAction_triggered();
    void at_fullscreenAction_triggered();
    void at_activityStopped();
    void at_activityStarted();
    void at_fpsChanged(qreal fps);

    void at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying);

    void at_display_widgetChanged(QnWorkbench::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_controlsWidget_deactivated();
    void at_controlsWidget_geometryChanged();

    void at_sliderItem_geometryChanged();
    void at_sliderShowButton_toggled(bool checked);

    void at_treeWidget_activated(const QnResourcePtr &resource);
    void at_treeItem_paintGeometryChanged();
    void at_treeHidingProcessor_hoverFocusLeft();
    void at_treeShowingProcessor_hoverEntered();
    void at_treeShowButton_toggled(bool checked);
    void at_treePinButton_toggled(bool checked);

    void at_tabBar_closeRequested(QnWorkbenchLayout *layout);
    void at_titleItem_geometryChanged();
    void at_titleItem_contextMenuRequested(QObject *target, QEvent *event);
    void at_titleShowButton_toggled(bool checked);

    void at_helpPinButton_toggled(bool checked);
    void at_helpShowButton_toggled(bool checked);
    void at_helpHidingProcessor_hoverFocusLeft();
    void at_helpShowingProcessor_hoverEntered();
    void at_helpItem_paintGeometryChanged();
    void at_helpWidget_showRequested();
    void at_helpWidget_hideRequested();

    void at_fpsItem_geometryChanged();

    void at_exportMediaRange(CLVideoCamera* camera, qint64 startTimeMs, qint64 endTimeMs);
    void at_exportFailed(QString errMessage);
    void at_exportFinished(QString fileName);

private:
    /* Global state. */

    /** Workbench display. */
    QnWorkbenchDisplay *m_display;

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;

    /** Ui elements instrument. */
    UiElementsInstrument *m_uiElementsInstrument;

    /** Fps counting instrument. */
    FpsCountingInstrument *m_fpsCountingInstrument;

    /** Activity listener instrument. */
    ActivityListenerInstrument *m_controlsActivityInstrument;

    /** Current flags. */
    Flags m_flags;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[QnWorkbench::ITEM_ROLE_COUNT];

    /** Widget that ui controls are placed on. */
    QGraphicsWidget *m_controlsWidget;

    /** Stored size of ui controls widget. */
    QRectF m_controlsWidgetRect;

    /** Whether the tree is opened. */
    bool m_treeOpened;

    /** Whether navigation slider is opened. */
    bool m_sliderOpened;

    /** Whether title bar is opened. */
    bool m_titleOpened;

    bool m_helpOpened;

    bool m_treeVisible;

    bool m_sliderVisible;

    bool m_titleVisible;

    bool m_helpVisible;

    bool m_titleUsed;

    bool m_inactive;

    GraphicsLabel *m_fpsItem;

    bool m_ignoreClickEvent;


    /* Slider-related state. */

    /** Navigation item. */
    NavigationItem *m_sliderItem;

    /** Hover processor that is used to change slider opacity when mouse is hovered over it. */
    HoverFocusProcessor *m_sliderOpacityProcessor;

    /** Animator for slider position. */
    VariantAnimator *m_sliderYAnimator;

    QnImageButtonWidget *m_sliderShowButton;

    AnimatorGroup *m_sliderOpacityAnimatorGroup;


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

    /** Whether the tree is pinned. */
    bool m_treePinned;

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

    bool m_helpPinned;


    /* Freespace-related state. */
    bool m_inFreespace;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchUi::Flags);

#endif // QN_WORKBENCH_UI_H
