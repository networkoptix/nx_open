#include "opacity_animator.h"

#include <QtWidgets/QGraphicsObject>

#include <utils/common/warnings.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/style/globals.h>

#include "animated.h"

#include <nx/vms/client/desktop/common/utils/accessor.h>

namespace {

static constexpr auto kOpacityAnimatorPropertyName = "_qn_opacityAnimator";
static constexpr auto kOpacityAnimatorTimeLimitMs = 200;

class OpacityAccessor: public nx::vms::client::desktop::AbstractAccessor
{
public:
    virtual QVariant get(const QObject* object) const override
    {
        return static_cast<const QGraphicsObject*>(object)->opacity();
    }

    virtual void set(QObject* object, const QVariant& value) const override
    {
        static_cast<QGraphicsObject*>(object)->setOpacity(value.toReal());
    }
};

} // namespace

Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

bool hasOpacityAnimator(QGraphicsObject* item)
{
    NX_ASSERT(item, "Item cannot be null here");
    QVariant animatorProperty = item->property(kOpacityAnimatorPropertyName);
    return animatorProperty.isValid()
        && animatorProperty.canConvert<VariantAnimator *>()
        && animatorProperty.value<VariantAnimator *>() != nullptr;
}

VariantAnimator* takeOpacityAnimator(QGraphicsObject* item)
{
    NX_ASSERT(item, "Item cannot be null here");
    if (!hasOpacityAnimator(item))
        return nullptr;

    auto animator = item->property(kOpacityAnimatorPropertyName).value<VariantAnimator*>();
    NX_ASSERT(animator);
    item->setProperty(kOpacityAnimatorPropertyName, {});
    return animator;
}

VariantAnimator* opacityAnimator(QGraphicsObject* item, qreal speed)
{
    NX_ASSERT(item, "Item cannot be null here");
    if (!item)
        return staticVariantAnimator();

    if (auto animator = item->property(kOpacityAnimatorPropertyName).value<VariantAnimator*>())
    {
        animator->setSpeed(speed);
        return animator;
    }

    AnimationTimer* animationTimer = nullptr;
    auto animatedBase = dynamic_cast<AnimatedBase*>(item);

    if (!animatedBase)
    {
        NX_ASSERT(item->scene(), "Cannot create opacity animator for an item not on a scene.");
        if (!item->scene())
            return staticVariantAnimator();

        animationTimer = InstrumentManager::animationTimer(item->scene());
        NX_ASSERT(animationTimer,
            "Cannot create opacity animator for an item on a scene that has no associated animation instrument.");
        if (!animationTimer)
            return staticVariantAnimator();
    }

    auto animator = new VariantAnimator(item);
    animator->setTimer(animationTimer);
    animator->setAccessor(new OpacityAccessor());
    animator->setTargetObject(item);
    animator->setTimeLimit(kOpacityAnimatorTimeLimitMs);
    animator->setSpeed(speed);
    if (animatedBase)
        animatedBase->registerAnimation(animator);

    item->setProperty(kOpacityAnimatorPropertyName, qVariantFromValue(animator));

    return animator;
}
