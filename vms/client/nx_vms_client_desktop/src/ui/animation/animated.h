// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtWidgets/QGraphicsItem>

#include <utils/common/forward.h>

#include "animation_timer.h"
#include "animation_timer_listener.h"

template<class Base, bool baseIsAnimated>
class Animated;

class AbstractAnimator;

/**
 * Support class for Animated template. In most cases there is no need to use this class directly.
 * The only possible use is to dynamic cast to it.
 */
class AnimatedBase
{
public:
    void registerAnimation(AbstractAnimator* animator);
    void registerAnimation(AnimationTimerListenerPtr listener);
    void unregisterAnimation(AnimationTimerListenerPtr listener);

private:
    AnimatedBase();
    virtual ~AnimatedBase();

    void updateScene(QGraphicsScene* scene);

    template<class Base, bool baseIsAnimated>
    friend class ::Animated; //< So that only this class can access our methods.

private:
    /**
     * A 'smart pointer' to current animation timer. AnimationTimer is not a QObject, this is why
     * in case we want to know of its destruction, we need to store a listener instead of a pointer
     * here. When AnimationTimer is destroyed, it will cleanup pointers to itself from all existing
     * listeners.
     */
    AnimationTimerListenerPtr m_animationTimerAccessor = AnimationTimerListener::create();

    /** Animations registered with an animated item. */
    std::vector<std::weak_ptr<AnimationTimerListener>> m_listeners;
};

/**
 * Convenience base class for graphics items that need to hook into the animation system provided
 * by the instrument manager.
 *
 * Derived class should use <tt>registerAnimation</tt> and <tt>unregisterAnimation</tt> functions
 * to register and unregister listeners that are to be animated. This class will properly link them
 * with an animation timer once one is available, and move to another animation timer in case the
 * item's scene is changed.
 */
template<class Base, bool baseIsAnimated = std::is_base_of<AnimatedBase, Base>::value>
class Animated: public Base, public AnimatedBase
{
public:
    QN_FORWARD_CONSTRUCTOR(Animated, Base, { AnimatedBase::updateScene(this->scene()); });

    using AnimatedBase::registerAnimation;
    using AnimatedBase::unregisterAnimation;

protected:
    virtual QVariant itemChange(
        QGraphicsItem::GraphicsItemChange change, const QVariant& value) override
    {
        if (change == QGraphicsItem::ItemSceneHasChanged)
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
class Animated<Base, true>: public Base
{
public:
    using Base::Base;
};
