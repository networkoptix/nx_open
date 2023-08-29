// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "margins_animator.h"

#include <QtCore/QMargins>
#include <QtWidgets/QGraphicsLinearLayout>

#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <ui/animation/animated.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <utils/common/checked_cast.h>

namespace {

static constexpr auto kMarginsAnimatorPropertyName = "_qn_marginsAnimator";

class MarginsAccessor: public nx::vms::client::desktop::AbstractAccessor
{
public:
    virtual QVariant get(const QObject* object) const override;
    virtual void set(QObject* object, const QVariant& value) const override;
};

QVariant MarginsAccessor::get(const QObject* object) const
{
    const auto widget = checked_cast<const QnViewportBoundWidget*>(object);
    if (!NX_ASSERT(widget))
        return {};

    const auto layout = widget->layout();
    if (!NX_ASSERT(layout))
        return {};

    qreal left, top, right, bottom;
    layout->getContentsMargins(&left, &top, &right, &bottom);

    return QVariant::fromValue(QMargins(left, top, right, bottom));
}

void MarginsAccessor::set(QObject* object, const QVariant& value) const
{
    auto widget = checked_cast<const QnViewportBoundWidget*>(object);
    if (!NX_ASSERT(widget))
        return;

    auto layout = widget->layout();
    if (!NX_ASSERT(layout))
        return;

    const QMargins margins = value.value<QMargins>();
    layout->setContentsMargins(margins.left(), margins.top(), margins.right(), margins.bottom());
}

} // namespace

Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

bool hasMarginsAnimator(QnViewportBoundWidget* item)
{
    if (!NX_ASSERT(item, "Item cannot be null here"))
        return false;

    QVariant animatorProperty = item->property(kMarginsAnimatorPropertyName);
    return animatorProperty.isValid()
        && animatorProperty.canConvert<VariantAnimator *>()
        && animatorProperty.value<VariantAnimator *>() != nullptr;
}

VariantAnimator* takeMarginsAnimator(QnViewportBoundWidget* item)
{
    if (!NX_ASSERT(item, "Item cannot be null here"))
        return nullptr;

    if (!hasMarginsAnimator(item))
        return nullptr;

    auto animator = item->property(kMarginsAnimatorPropertyName).value<VariantAnimator*>();
    NX_ASSERT(animator);
    item->setProperty(kMarginsAnimatorPropertyName, {});
    return animator;
}

VariantAnimator* marginsAnimator(QnViewportBoundWidget* item, qreal speed)
{
    if (!NX_ASSERT(item, "Item cannot be null here"))
        return staticVariantAnimator();

    if (auto animator = item->property(kMarginsAnimatorPropertyName).value<VariantAnimator*>())
    {
        animator->setSpeed(speed);
        return animator;
    }

    AnimationTimer* animationTimer = nullptr;
    auto animatedBase = dynamic_cast<AnimatedBase*>(item);

    if (!animatedBase)
    {
        if (!NX_ASSERT(item->scene(), "Cannot create margins animator for an item not on a scene."))
            return staticVariantAnimator();

        animationTimer = InstrumentManager::animationTimer(item->scene());

        if (!NX_ASSERT(animationTimer, "Cannot create margins animator for an "
            "item on a scene that has no associated animation instrument."))
        {
            return staticVariantAnimator();
        }
    }

    auto animator = new VariantAnimator(item);
    animator->setTimer(animationTimer);
    animator->setAccessor(new MarginsAccessor());
    animator->setTargetObject(item);
    animator->setSpeed(speed);
    if (animatedBase)
        animatedBase->registerAnimation(animator);

    item->setProperty(kMarginsAnimatorPropertyName, QVariant::fromValue(animator));

    return animator;
}
