// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "opacity_animator.h"

#include <QtWidgets/QWidget>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/utils/widget_opacity_mixin.h>

#include "animated.h"

#include <nx/vms/client/desktop/common/utils/accessor.h>

using namespace nx::vms::client::desktop;

namespace {

static constexpr auto kOpacityAnimatorPropertyName = "_qn_widgetOpacityAnimator";
static constexpr auto kOpacityAnimatorTimeLimitMs = 200;

} // namespace

Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

bool hasOpacityAnimator(QWidget* widget)
{
    NX_ASSERT(widget, "Widget cannot be null here");
    QVariant animatorProperty = widget->property(kOpacityAnimatorPropertyName);
    return animatorProperty.isValid()
        && animatorProperty.canConvert<VariantAnimator *>()
        && animatorProperty.value<VariantAnimator *>() != nullptr;
}

VariantAnimator* takeWidgetOpacityAnimator(QWidget* widget)
{
    NX_ASSERT(widget, "Widget cannot be null here");
    if (!hasOpacityAnimator(widget))
        return nullptr;

    auto animator = widget->property(kOpacityAnimatorPropertyName).value<VariantAnimator*>();
    NX_ASSERT(animator);
    widget->setProperty(kOpacityAnimatorPropertyName, {});
    return animator;
}

VariantAnimator* widgetOpacityAnimator(QWidget* widget, AnimationTimer* timer, qreal speed)
{
    NX_ASSERT(widget, "Widget cannot be null here");
    if (!widget)
        return staticVariantAnimator();

    if (auto animator = widget->property(kOpacityAnimatorPropertyName).value<VariantAnimator*>())
    {
        animator->setSpeed(speed);
        return animator;
    }

    auto animator = new VariantAnimator(widget);
    animator->setTimer(timer);
    animator->setAccessor(new PropertyAccessor(opacityPropertyName()));
    animator->setTargetObject(widget);
    animator->setTimeLimit(kOpacityAnimatorTimeLimitMs);
    animator->setSpeed(speed);

    widget->setProperty(kOpacityAnimatorPropertyName, QVariant::fromValue(animator));

    return animator;
}
