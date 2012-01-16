#include "widget_opacity_animator.h"
#include <QGraphicsObject>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <utils/common/warnings.h>
#include <ui/skin/globals.h>

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

Q_DECLARE_METATYPE(VariantAnimator *);

Q_GLOBAL_STATIC(VariantAnimator, staticVariantAnimator);

VariantAnimator *opacityAnimator(QGraphicsObject *item, qreal speed) {
    if(item == NULL) {
        qnNullWarning(item);
        return staticVariantAnimator();
    }

    VariantAnimator *animator = item->property(opacityAnimatorPropertyName).value<VariantAnimator *>();
    if(animator != NULL)
        return animator;

    if(item->scene() == NULL) {
        qnWarning("Cannot create opacity animator for an item not on a scene.");
        return staticVariantAnimator();
    }

    const QList<InstrumentManager *> managers =  InstrumentManager::managersOf(item->scene());
    if(managers.empty()) {
        qnWarning("Cannot create opacity animator for an item on a scene with no instrument managers.");
        return staticVariantAnimator();
    }

    AnimationInstrument *animationInstrument = managers[0]->instrument<AnimationInstrument>();
    if(animationInstrument == NULL) {
        qnWarning("Cannot create opacity animator for an item on a scene that has no associated animation instrument.");
        return staticVariantAnimator();
    }

    animator = new VariantAnimator(item);
    animator->setTimer(animationInstrument->animationTimer());
    animator->setAccessor(new OpacityAccessor());
    animator->setTargetObject(item);
    animator->setTimeLimit(Globals::opacityChangePeriod());
    animator->setSpeed(speed);

    item->setProperty(opacityAnimatorPropertyName, QVariant::fromValue<VariantAnimator *>(animator));

    return animator;
}

