#include "workbench_animations.h"

#include <ui/animation/variant_animator.h>
#include <ui/animation/widget_animator.h>

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {

namespace {

int idx(Animations::Id id)
{
    return static_cast<int>(id);
}

} // namespace


Animations::Animations(QObject* parent):
    base_type(parent)
{
    auto setup =
        [this](Id id, QEasingCurve::Type easing, int timeLimit)
        {
            m_easing[idx(id)] = easing;
            m_timeLimit[idx(id)] = timeLimit;
        };

    setup(Id::TimelineExpanding, QEasingCurve::InOutQuad, 240);
    setup(Id::TimelineCollapsing, QEasingCurve::OutQuad, 240);
    setup(Id::TimelineTooltipShow, QEasingCurve::InOutQuad, 160);
    setup(Id::TimelineTooltipHide, QEasingCurve::InOutQuad, 160);
    setup(Id::ResourcesPanelExpanding, QEasingCurve::InOutQuad, 300);
    setup(Id::ResourcesPanelCollapsing, QEasingCurve::OutQuad, 300);
    setup(Id::NotificationsPanelExpanding, QEasingCurve::InOutQuad, 300);
    setup(Id::NotificationsPanelCollapsing, QEasingCurve::OutQuad, 300);
    setup(Id::CalendarExpanding, QEasingCurve::OutQuad, 50);
    setup(Id::CalendarCollapsing, QEasingCurve::InQuad, 50);
    setup(Id::SceneItemGeometryChange, QEasingCurve::InOutQuad, 200);
    setup(Id::SceneZoomIn, QEasingCurve::InOutQuad, 500);
    setup(Id::SceneZoomOut, QEasingCurve::InOutQuad, 500);
}

Animations::~Animations()
{

}

void Animations::setupAnimator(VariantAnimator* animator, Id id)
{
    NX_ASSERT(animator);
    if (!animator)
        return;

    animator->setEasingCurve(m_easing[idx(id)]);
    animator->setTimeLimit(m_timeLimit[idx(id)]);
}

void Animations::setupAnimator(WidgetAnimator* animator, Id id)
{
    NX_ASSERT(animator);
    if (!animator)
        return;

    animator->setEasingCurve(m_easing[idx(id)]);
    animator->setTimeLimit(m_timeLimit[idx(id)]);
}

QEasingCurve::Type Animations::easing(Id id) const
{
    return m_easing[idx(id)];
}

void Animations::setEasing(Id id, QEasingCurve::Type value)
{
    m_easing[idx(id)] = value;
}

int Animations::timeLimit(Id id) const
{
    return m_timeLimit[idx(id)];
}

void Animations::setTimeLimit(Id id, int value)
{
    m_timeLimit[idx(id)] = value;
}

} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
