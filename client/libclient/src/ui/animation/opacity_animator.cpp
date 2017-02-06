#include "opacity_animator.h"

#include <QtWidgets/QGraphicsObject>

#include <utils/common/warnings.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/style/globals.h>

#include "animated.h"

namespace {
    const char *opacityAnimatorPropertyName = "_qn_opacityAnimator";

    class OpacityAccessor: public AbstractAccessor {
    public:
        virtual QVariant get(const QObject *object) const override {
            return static_cast<const QGraphicsObject *>(object)->opacity();
        }

        virtual void set(QObject *object, const QVariant &value) const override {
            static_cast<QGraphicsObject *>(object)->setOpacity(value.toReal());
        }
    };

    static const int kOpacityTimeLimitMs = 200;

} // anonymous namespace


bool hasOpacityAnimator(QGraphicsObject *item)
{
    QVariant animatorProperty = item->property(opacityAnimatorPropertyName);
    return animatorProperty.isValid()
        && animatorProperty.canConvert<VariantAnimator *>()
        && animatorProperty.value<VariantAnimator *>() != nullptr;
}


Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

VariantAnimator *opacityAnimator(QGraphicsObject *item, qreal speed)
{
    NX_ASSERT(item != nullptr, Q_FUNC_INFO, "Item cannot be null here");
    if(item == NULL)
        return staticVariantAnimator();

    VariantAnimator *animator = item->property(opacityAnimatorPropertyName).value<VariantAnimator *>();
    if (animator)
    {
        animator->setSpeed(speed);
        return animator;
    }

    AnimationTimer *animationTimer = NULL;
    AnimatedBase *animatedBase = dynamic_cast<AnimatedBase *>(item);

    if(!animatedBase)
    {
        NX_ASSERT(item->scene() != nullptr, Q_FUNC_INFO, "Cannot create opacity animator for an item not on a scene.");
        if(!item->scene())
            return staticVariantAnimator();

        animationTimer = InstrumentManager::animationTimer(item->scene());
        NX_ASSERT(animationTimer != nullptr, Q_FUNC_INFO,
            "Cannot create opacity animator for an item on a scene that has no associated animation instrument.");
        if(!animationTimer)
            return staticVariantAnimator();
    }

    animator = new VariantAnimator(item);
    animator->setTimer(animationTimer);
    animator->setAccessor(new OpacityAccessor());
    animator->setTargetObject(item);
    animator->setTimeLimit(kOpacityTimeLimitMs);
    animator->setSpeed(speed);
    if(animatedBase)
        animatedBase->registerAnimation(animator);

    item->setProperty(opacityAnimatorPropertyName, QVariant::fromValue<VariantAnimator *>(animator));

    return animator;
}

