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


    /* Ui-related state. */

    /** Widget that ui controls are placed on. */
    QGraphicsWidget *m_controlsWidget;

    QSizeF m_controlsWidgetSize;

    /** Navigation item. */
    NavigationItem *m_navigationItem;

    /** Navigation tree widget. */
    NavigationTreeWidget *m_treeWidget;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget *m_treeItem;

    QGraphicsWidget *m_treeBackgroundItem;

    QnImageButtonWidget *m_treeShowButton;

    QnImageButtonWidget *m_treePinButton;

    /** Whether navigation tree is visible. */
    bool m_treeVisible;

    bool m_sliderVisible;

    bool m_treePinned;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[QnWorkbench::ITEM_ROLE_COUNT];

    /** Hover opacity item for tree widget. */
    HoverFocusProcessor *m_treeHidingProcessor;

    HoverFocusProcessor *m_treeShowingProcessor;

    HoverFocusProcessor *m_treeOpacityProcessor;

    HoverFocusProcessor *m_sliderOpacityProcessor;

    AnimatorGroup *m_treeOpacityAnimatorGroup;
    VariantAnimator *m_treePositionAnimator;
    VariantAnimator *m_sliderPositionAnimator;
};

#endif // QN_WORKBENCH_UI_H
