#ifndef QN_ANIMATED_H
#define QN_ANIMATED_H

#include <QtCore/QSet>
#include <QtCore/QScopedPointer>
#include <QtGui/QGraphicsItem>

#include <utils/common/forward.h>

#include "animation_timer.h"
#include "animation_timer_listener.h"

template<class Base, bool baseIsAnimated>
class Animated;

namespace detail {
    class AnimatedBase {
    private:
        AnimatedBase();
        virtual ~AnimatedBase();

        void updateScene(QGraphicsScene *scene);

        void registerAnimation(AnimationTimerListener *listener);

        void unregisterAnimation(AnimationTimerListener *listener);

        template<class Base, bool baseIsAnimated>
        friend class ::Animated; /* So that only this class can access our methods. */

    private:
        QScopedPointer<AnimationTimerListener> m_listener;
        QSet<AnimationTimerListener *> m_listeners;
    };

    template<class Base>
    struct base_is_animated {
        typedef char true_type;
        typedef struct { char dummy[2]; } false_type;

        static true_type check(AnimatedBase *);
        static false_type check(...);

        enum {
            value = (sizeof(check(static_cast<Base *>(NULL))) == sizeof(true_type))
        };
    };

} // namespace detail


/**
 * Convenience base class for graphics items that need to hook into the 
 * animation system provided by instrument manager.
 * 
 * Derived class should use <tt>registerAnimation</tt> and <tt>unregisterAnimation</tt>
 * functions to register and unregister listeners that are to be animated.
 * This class will properly link them with an animation timer once one is 
 * available, and move to another animation timer in case the item's scene is
 * changed.
 */
template<class Base, bool baseIsAnimated = detail::base_is_animated<Base>::value>
class Animated: public Base, public detail::AnimatedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Animated, Base, { updateScene(this->scene()); });

    using detail::AnimatedBase::registerAnimation;
    using detail::AnimatedBase::unregisterAnimation;

protected:
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        if(change == QGraphicsItem::ItemSceneHasChanged)
            updateScene(this->scene());

        return Base::itemChange(change, value);
    }
};


/**
 * Specialization that prevents creation of two animation data instances in
 * a single class, even if it was tagged as 'Animated' several times 
 * (e.g. one of its bases is animated).
 */
template<class Base>
class Animated<Base, true>: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Animated, Base, {});
};


#endif // QN_ANIMATED_H
