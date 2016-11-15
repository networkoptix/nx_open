#include "workbench_animations.h"

#include <ui/animation/variant_animator.h>
#include <ui/animation/widget_animator.h>

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {

namespace {

static const qreal kDefaultSpeed = 0.01;

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

    setup(Id::TimelineShow, QEasingCurve::InOutQuad, 240);
    setup(Id::TimelineHide, QEasingCurve::OutQuad, 240);
    setup(Id::TimelineTooltipShow, QEasingCurve::InOutQuad, 160);
    setup(Id::TimelineTooltipHide, QEasingCurve::InOutQuad, 160);
    setup(Id::ResourcesPanelShow, QEasingCurve::InOutQuad, 300);
    setup(Id::ResourcesPanelHide, QEasingCurve::OutQuad, 300);
    setup(Id::NotificationsPanelShow, QEasingCurve::InOutQuad, 300);
    setup(Id::NotificationsPanelHide, QEasingCurve::OutQuad, 300);
    setup(Id::CalendarShow, QEasingCurve::OutQuad, 50);
    setup(Id::CalendarHide, QEasingCurve::InQuad, 50);
    setup(Id::SceneItemGeometryChange, QEasingCurve::InOutQuad, 200);
    setup(Id::SceneZoomIn, QEasingCurve::InOutQuad, 500);
    setup(Id::SceneZoomOut, QEasingCurve::InOutQuad, 500);
    setup(Id::ItemOverlayShow, QEasingCurve::InOutQuad, 200);
    setup(Id::ItemOverlayHide, QEasingCurve::InOutQuad, 200);
}

Animations::~Animations()
{

}

void Animations::setupAnimator(VariantAnimator* animator, Id id)
{
    NX_ASSERT(animator);
    if (!animator)
        return;

    /* Set base speed to very small value, so time limit will be default option to setup speed. */
    animator->setSpeed(kDefaultSpeed);
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
