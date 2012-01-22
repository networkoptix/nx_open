#ifndef QN_WORKBENCH_UI_H
#define QN_WORKBENCH_UI_H

#include <QObject>
#include <ui/common/scene_utility.h>
#include "workbench.h"

class CLVideoCamera;

class InstrumentManager;
class UiElementsInstrument;
class VariantAnimator;
class AnimatorGroup;
class HoverFocusProcessor;

class NavigationItem;
class NavigationTreeWidget;

class QnWorkbenchDisplay;
class QnImageButtonWidget;
class QnResourceWidget;
class QnMaskedProxyWidget;


class QnWorkbenchUi: public QObject, protected SceneUtility {
    Q_OBJECT;

public:
    QnWorkbenchUi(QnWorkbenchDisplay *display, QObject *parent = NULL);
    virtual ~QnWorkbenchUi();

    QnWorkbenchDisplay *display() const;

    QnWorkbench *workbench() const;

    NavigationTreeWidget *treeWidget() const {
        return m_treeWidget;
    }

public Q_SLOTS:
    void setTreeVisible(bool visible, bool animate = true);
    void setSliderVisible(bool visible, bool animate = true);
    void toggleTreeVisible();

protected:
    void updateViewportMargins();

    void updateTreeGeometry();

    static QRectF updatedTreeGeometry(const QRectF &controlGeometry, const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry);

protected Q_SLOTS:
    void at_display_widgetChanged(QnWorkbench::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_controlsWidget_deactivated();
    void at_controlsWidget_geometryChanged();

    void at_navigationItem_geometryChanged();
    void at_navigationItem_actualCameraChanged(CLVideoCamera *camera);
    void at_sliderOpacityProcessor_hoverEntered();
    void at_sliderOpacityProcessor_hoverLeft();

    void at_treeItem_paintGeometryChanged();
    void at_treeHidingProcessor_hoverFocusLeft();
    void at_treeShowingProcessor_hoverEntered();
    void at_treeOpacityProcessor_hoverLeft();
    void at_treeOpacityProcessor_hoverEntered();
    void at_treeShowButton_toggled(bool checked);
    void at_treePinButton_toggled(bool checked);

private:
    /* Global state. */

    /** Workbench display. */
    QnWorkbenchDisplay *m_display;

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;

    /** Ui elements instrument. */
    UiElementsInstrument *m_uiElementsInstrument;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[QnWorkbench::ITEM_ROLE_COUNT];

    /** Widget that ui controls are placed on. */
    QGraphicsWidget *m_controlsWidget;

    /** Stored size of ui controls widget. */
    QSizeF m_controlsWidgetSize;


    /* Slider-related state. */

    /** Navigation item. */
    NavigationItem *m_sliderItem;

    /** Whether navigation slider is visible. */
    bool m_sliderVisible;

    /** Hover processor that is used to change slider opacity when mouse is hovered over it. */
    HoverFocusProcessor *m_sliderOpacityProcessor;

    /** Animator for slider position. */
    VariantAnimator *m_sliderYAnimator;


    /* Tree-related state. */

    /** Navigation tree widget. */
    NavigationTreeWidget *m_treeWidget;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget *m_treeItem;

    /** Item that provides background for the tree. */
    QGraphicsWidget *m_treeBackgroundItem;

    /** Button to show/hide the tree. */
    QnImageButtonWidget *m_treeShowButton;

    /** Button to pin the tree. */
    QnImageButtonWidget *m_treePinButton;

    /** Whether the tree is visible. */
    bool m_treeVisible;

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

    /** Background widget for the title bar. */
    QGraphicsWidget *m_titleItem;

    VariantAnimator *m_titleYAnimator;
};

#endif // QN_WORKBENCH_UI_H
