#include "opacity_animator.h"

#include <QtGui/QGraphicsObject>

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

} // anonymous namespace

Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

VariantAnimator *opacityAnimator(QGraphicsObject *item, qreal speed) {
    if(item == NULL) {
        qnNullWarning(item);
        return staticVariantAnimator();
    }

    VariantAnimator *animator = item->property(opacityAnimatorPropertyName).value<VariantAnimator *>();
    if(animator) {
        animator->setSpeed(speed);
        return animator;
    }

    AnimationTimer *animationTimer = NULL;
    AnimatedBase *animatedBase = dynamic_cast<AnimatedBase *>(item);

    if(!animatedBase) {
        if(!item->scene()) {
            qnWarning("Cannot create opacity animator for an item not on a scene.");
            return staticVariantAnimator();
        }
            
        animationTimer = InstrumentManager::animationTimerOf(item->scene());
        if(!animationTimer) {
            qnWarning("Cannot create opacity animator for an item on a scene that has no associated animation instrument.");
            return staticVariantAnimator();
        }
    }

    animator = new VariantAnimator(item);
    animator->setTimer(animationTimer);
    animator->setAccessor(new OpacityAccessor());
    animator->setTargetObject(item);
    animator->setTimeLimit(qnGlobals->opacityChangePeriod());
    animator->setSpeed(speed);
    if(animatedBase)
        animatedBase->registerAnimation(animator);

    item->setProperty(opacityAnimatorPropertyName, QVariant::fromValue<VariantAnimator *>(animator));

    return animator;
}

