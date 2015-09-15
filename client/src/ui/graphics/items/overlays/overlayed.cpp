#include "overlayed.h"

#include <QtWidgets/QGraphicsScene>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>

#include <utils/common/warnings.h>

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

void detail::OverlayedBase::addOverlayWidget(QGraphicsWidget *widget, OverlayVisibility visibility, bool autoRotate, bool bindToViewport, qreal z) {
    if(!widget) {
        qnNullWarning(widget);
        return;
    }

    QnViewportBoundWidget *boundWidget = dynamic_cast<QnViewportBoundWidget *>(widget);
    if(bindToViewport && !boundWidget) {
        QGraphicsLinearLayout *boundLayout = new QGraphicsLinearLayout();
        boundLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
        boundLayout->addItem(widget);

        boundWidget = new QnViewportBoundWidget();
        boundWidget->setLayout(boundLayout);
        boundWidget->setAcceptedMouseButtons(0);
    }
    QGraphicsWidget *childWidget = boundWidget ? boundWidget : widget;
    childWidget->setParentItem(m_widget);
    childWidget->setZValue(z);

    QnFixedRotationTransform *rotationTransform = NULL;
    if(autoRotate) {
        rotationTransform = new QnFixedRotationTransform(widget);
        rotationTransform->setTarget(widget);
        rotationTransform->setAngle(m_overlayRotation);
    }

    OverlayWidget overlay;
    overlay.visibility = visibility;
    overlay.widget = widget;
    overlay.childWidget = childWidget;
    overlay.boundWidget = boundWidget;
    overlay.rotationTransform = rotationTransform;
    m_overlayWidgets.append(overlay);

    updateOverlayWidgetsGeometry();
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

void detail::OverlayedBase::setOverlayWidgetVisibility(QGraphicsWidget *widget, OverlayVisibility visibility) {
    int index = overlayWidgetIndex(widget);
    if(index == -1)
        return;

    if(m_overlayWidgets[index].visibility == visibility)
        return;

    m_overlayWidgets[index].visibility = visibility;
    updateOverlayWidgetsVisibility();
}

bool detail::OverlayedBase::isOverlayVisible() const {
    return m_overlayVisible;
}

void detail::OverlayedBase::setOverlayVisible(bool visible, bool animate) {
    if (m_overlayVisible == visible)
        return;

    m_overlayVisible = visible;
    updateOverlayWidgetsVisibility(animate);
}

void detail::OverlayedBase::updateOverlayWidgetsGeometry() {
    foreach(const OverlayWidget &overlay, m_overlayWidgets) {
        QSizeF size = m_widget->size();

        if(overlay.rotationTransform) {
            overlay.rotationTransform->setAngle(m_overlayRotation);

            if(m_overlayRotation == Qn::Angle90 || m_overlayRotation == Qn::Angle270)
                size.transpose();
        }

        if(overlay.boundWidget) {
            overlay.boundWidget->setFixedSize(size);
        } else {
            overlay.widget->resize(size);
        }
    }
}

void detail::OverlayedBase::updateOverlayWidgetsVisibility(bool animate) {
    foreach(const OverlayWidget &overlay, m_overlayWidgets) {
        if(overlay.visibility == UserVisible)
            continue;

        bool visible = m_overlayVisible;

        if(overlay.visibility == Invisible) {
            visible = false;
        } else if(overlay.visibility == Visible) {
            visible = true;
        } 
        setOverlayWidgetVisible(overlay.widget, visible, animate);
    }
}

void detail::OverlayedBase::setOverlayWidgetVisible(QGraphicsWidget* widget, bool visible /*= true*/, bool animate /*= true*/) {
    qreal opacity = visible ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(widget, 1.0)->animateTo(opacity);
    } else {
        widget->setOpacity(opacity);
    }
}
