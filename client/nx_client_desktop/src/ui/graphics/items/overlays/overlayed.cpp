#include "overlayed.h"

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>
#include <nx/utils/math/fuzzy.h>

#include <utils/common/warnings.h>

detail::OverlayedBase::OverlayWidget::OverlayWidget()
    : visibility(OverlayVisibility::Invisible)
    , widget(nullptr)
    , childWidget(nullptr)
    , boundWidget(nullptr)
    , rotationTransform(nullptr)
{}


void detail::OverlayedBase::initOverlayed(QGraphicsWidget *widget) {
    m_widget = widget;
    m_overlayVisible = false;
    m_overlayRotation = Qn::Angle0;
}

void detail::OverlayedBase::updateOverlaysRotation() {
    Qn::FixedRotation overlayRotation = fixedRotationFromDegrees(m_widget->rotation());
    if(overlayRotation != m_overlayRotation) {
        m_overlayRotation = overlayRotation;
        updateOverlayWidgetsGeometry();
    }
}


int detail::OverlayedBase::overlayWidgetIndex(QGraphicsWidget *widget) const {
    for(int i = 0; i < m_overlayWidgets.size(); i++)
        if(m_overlayWidgets[i].widget == widget)
            return i;
    return -1;
}

void detail::OverlayedBase::addOverlayWidget(QGraphicsWidget *widget
    , const OverlayParams &params)
{
    if(!widget)
    {
        qnNullWarning(widget);
        return;
    }

    QnViewportBoundWidget *boundWidget = dynamic_cast<QnViewportBoundWidget *>(widget);
    if(params.bindToViewport && !boundWidget)
    {
        QGraphicsLinearLayout *boundLayout = new QGraphicsLinearLayout();
        boundLayout->setContentsMargins(params.margins.left()
            , params.margins.top(), params.margins.right(), params.margins.bottom());
        boundLayout->addItem(widget);

        boundWidget = new QnViewportBoundWidget();
        boundWidget->setLayout(boundLayout);
        boundWidget->setAcceptedMouseButtons(0);
    }

    QGraphicsWidget *childWidget = boundWidget ? boundWidget : widget;
    childWidget->setParentItem(m_widget);
    childWidget->setZValue(params.z);

    QnFixedRotationTransform *rotationTransform = NULL;
    if(params.autoRotate)
    {
        rotationTransform = new QnFixedRotationTransform(widget);
        rotationTransform->setTarget(widget);
        rotationTransform->setAngle(m_overlayRotation);
    }

    OverlayWidget overlay;
    overlay.visibility = params.visibility;
    overlay.widget = widget;
    overlay.childWidget = childWidget;
    overlay.boundWidget = boundWidget;
    overlay.rotationTransform = rotationTransform;
    m_overlayWidgets.append(overlay);

    updateOverlayWidgetsGeometry();
    updateOverlayWidgetsVisibility(false);
}

void detail::OverlayedBase::removeOverlayWidget(QGraphicsWidget *widget) {
    int index = overlayWidgetIndex(widget);
    if(index == -1)
        return;

    const OverlayWidget &overlay = m_overlayWidgets[index];
    overlay.widget->setParentItem(NULL);
    if(overlay.boundWidget && overlay.boundWidget != overlay.widget)
        delete overlay.boundWidget;

    m_overlayWidgets.removeAt(index);
}

detail::OverlayedBase::OverlayVisibility detail::OverlayedBase::overlayWidgetVisibility(QGraphicsWidget *widget) const {
    int index = overlayWidgetIndex(widget);
    return index == -1 ? Invisible : m_overlayWidgets[index].visibility;
}

void detail::OverlayedBase::setOverlayWidgetVisibility(QGraphicsWidget *widget, OverlayVisibility visibility, bool animate)
{
    int index = overlayWidgetIndex(widget);
    if(index == -1)
        return;

    if(m_overlayWidgets[index].visibility == visibility)
        return;

    m_overlayWidgets[index].visibility = visibility;
    updateOverlayWidgetsVisibility(animate);
}

bool detail::OverlayedBase::isOverlayVisible() const {
    return m_overlayVisible;
}

void detail::OverlayedBase::setOverlayVisible(bool visible, bool animate)
{
    if (m_overlayVisible == visible)
        return;

    m_overlayVisible = visible;
    updateOverlayWidgetsVisibility(animate);
}

void detail::OverlayedBase::updateOverlayWidgetsGeometry()
{
    for(const OverlayWidget &overlay: m_overlayWidgets)
    {
        QSizeF size = m_widget->size();

        if (overlay.rotationTransform)
        {
            overlay.rotationTransform->setAngle(m_overlayRotation);

            if(m_overlayRotation == Qn::Angle90 || m_overlayRotation == Qn::Angle270)
                size.transpose();
        }

        if (overlay.boundWidget)
            overlay.boundWidget->setFixedSize(size);
        else
            overlay.widget->resize(size);
    }
}

void detail::OverlayedBase::updateOverlayWidgetsVisibility(bool animate)
{
    for(const OverlayWidget &overlay: m_overlayWidgets)
    {
        if (overlay.visibility == UserVisible)
            continue;

        bool visible = m_overlayVisible;

        if (overlay.visibility == Invisible)
            visible = false;
        else if(overlay.visibility == Visible)
            visible = true;

        setOverlayWidgetVisible(overlay.widget, visible, animate);
    }
}

void detail::OverlayedBase::setOverlayWidgetVisible(QGraphicsWidget* widget, bool visible /*= true*/, bool animate /*= true*/)
{
    if (!widget)
        return;

    const qreal opacity = visible ? 1.0 : 0.0;
    if (animate)
    {
        using namespace nx::client::desktop::ui::workbench;
        auto animator = opacityAnimator(widget);

        const bool sameTarget = animator->isRunning()
            && qFuzzyEquals(animator->targetValue().toReal(), opacity);

        if (!sameTarget)
        {
            animator->stop();

            qnWorkbenchAnimations->setupAnimator(animator, visible
                ? Animations::Id::ItemOverlayShow
                : Animations::Id::ItemOverlayHide);

            animator->animateTo(opacity);
        }
    }
    else
    {
        if (hasOpacityAnimator(widget))
            opacityAnimator(widget)->stop();

        widget->setOpacity(opacity);
    }

    NX_ASSERT(isOverlayWidgetVisible(widget) == visible, Q_FUNC_INFO, "Validate checking function");
}

bool detail::OverlayedBase::isOverlayWidgetVisible(QGraphicsWidget* widget)
{
    if (!widget)
        return false;

    if (hasOpacityAnimator(widget))
    {
        auto animator = opacityAnimator(widget);
        if (animator->isRunning())
            return !qFuzzyIsNull(animator->targetValue().toDouble());
    }
    return !qFuzzyIsNull(widget->opacity());
}

detail::OverlayParams::OverlayParams(OverlayedBase::OverlayVisibility visibility
    , bool autoRotate
    , bool bindToViewport
    , qreal z
    , const QMarginsF &margins)

    : visibility(visibility)
    , autoRotate(autoRotate)
    , bindToViewport(bindToViewport)
    , z(z)
    , margins(margins)
{}