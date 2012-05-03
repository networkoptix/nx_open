#ifndef QN_ANIMATED_H
#define QN_ANIMATED_H

#include <QtGui/QGraphicsItem>
#include "animation_timer.h"
#include "animation_timer_listener.h"

namespace detail {
    class AnimatedBase {
    protected:
        AnimatedBase(): m_timer(NULL) {}

        void updateScene(QGraphicsScene *scene);

        void registerAnimation(AnimationTimerListener *listener);

        void unregisterAnimation(AnimationTimerListener *listener);

    private:
        void ensureTimer();
        void ensureGuard();

    private:
        AnimationTimer *m_timer;
        QScopedPointer<AnimationTimer> m_guard;
    };

} // namespace detail


template<class Base>
class Animated: public Base, protected detail::AnimatedBase {
public:
    template<class T0>
    Animated(const T0 &arg0): Base(arg0) {
        updateScene(this->scene());
    }

    template<class T0, class T1>
    Animated(const T0 &arg0, const T1 &arg1): Base(arg0, arg1) {
        updateScene(this->scene());
    }

    virtual ~Animated() {
        updateScene(NULL);
    }

    using detail::AnimatedBase::registerAnimation;
    using detail::AnimatedBase::unregisterAnimation;

protected:
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        if(change == QGraphicsItem::ItemSceneHasChanged)
            updateScene(this->scene());

        return Base::itemChange(change, value);
    }

};

#endif // QN_ANIMATED_H
