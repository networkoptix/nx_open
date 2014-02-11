#ifndef QN_ANIMATED_H
#define QN_ANIMATED_H

#ifndef Q_MOC_RUN
#include <boost/type_traits/is_base_of.hpp>
#endif

#include <QtCore/QSet>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QGraphicsItem>

#include <utils/common/forward.h>

#include "animation_timer.h"
#include "animation_timer_listener.h"

template<class Base, bool baseIsAnimated>
class Animated;

/**
 * Support class for <tt>Animated</tt> template. In most cases there is no
 * need to use this class directly. The only possible use is to 
 * <tt>dynamic_cast</tt> to it.
 */
class AnimatedBase {
public:
    void registerAnimation(AnimationTimerListener *listener);

    void unregisterAnimation(AnimationTimerListener *listener);

private:
    AnimatedBase();
    virtual ~AnimatedBase();

    void updateScene(QGraphicsScene *scene);

    template<class Base, bool baseIsAnimated>
    friend class ::Animated; /* So that only this class can access our methods. */

private:
    /** A 'smart pointer' to current animation timer. 
     * <tt>AnimationTimer<tt> is not a <tt>QObject</tt>, this is why in case
     * we want to know of its destruction, we need to store a listener instead 
     * of a pointer here. */
    QScopedPointer<AnimationTimerListener> m_listener;

    /** Set of animations registered with an animated item. */
    QSet<AnimationTimerListener *> m_listeners;
};


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
template<class Base, bool baseIsAnimated = boost::is_base_of<AnimatedBase, Base>::value>
class Animated: public Base, public AnimatedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Animated, Base, { AnimatedBase::updateScene(this->scene()); });

    using AnimatedBase::registerAnimation;
    using AnimatedBase::unregisterAnimation;

protected:
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        if(change == QGraphicsItem::ItemSceneHasChanged)
            AnimatedBase::updateScene(this->scene());

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
