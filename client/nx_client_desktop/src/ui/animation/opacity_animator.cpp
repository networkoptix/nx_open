#include "opacity_animator.h"

#include <QtWidgets/QGraphicsObject>

#include <utils/common/warnings.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/style/globals.h>

#include "animated.h"

namespace {

const auto opacityAnimatorPropertyName = "_qn_opacityAnimator";

class OpacityAccessor: public AbstractAccessor
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

static const int kOpacityTimeLimitMs = 200;

} // anonymous namespace

Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

bool hasOpacityAnimator(QGraphicsObject* item)
{
    NX_ASSERT(item, "Item cannot be null here");
    QVariant animatorProperty = item->property(opacityAnimatorPropertyName);
    return animatorProperty.isValid()
        && animatorProperty.canConvert<VariantAnimator *>()
        && animatorProperty.value<VariantAnimator *>() != nullptr;
}

VariantAnimator* takeOpacityAnimator(QGraphicsObject* item)
{
    NX_ASSERT(item, "Item cannot be null here");
    if (!hasOpacityAnimator(item))
        return nullptr;

    auto animator = item->property(opacityAnimatorPropertyName).value<VariantAnimator*>();
    NX_ASSERT(animator);
    item->setProperty(opacityAnimatorPropertyName, {});
    return animator;
}

VariantAnimator* opacityAnimator(QGraphicsObject* item, qreal speed)
{
    NX_ASSERT(item, "Item cannot be null here");
    if (!item)
        return staticVariantAnimator();

    if (auto animator = item->property(opacityAnimatorPropertyName).value<VariantAnimator*>())
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
    animator->setTimeLimit(kOpacityTimeLimitMs);
    animator->setSpeed(speed);
    if (animatedBase)
        animatedBase->registerAnimation(animator);

    item->setProperty(opacityAnimatorPropertyName, qVariantFromValue(animator));

    return animator;
}



