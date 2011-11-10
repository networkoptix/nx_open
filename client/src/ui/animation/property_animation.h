/*
 *  property_animation.h
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#ifndef _PROPERTY_ANIMATION_WRAPPER_H_
#define _PROPERTY_ANIMATION_WRAPPER_H_

#include <QtCore/QPropertyAnimation>

#include "animation.h"

class PropertyAnimationWrapper;

/**
  * @class AnimationManager
  *
  * Manage concurrent animation to not allow multiple repaints
  */
class AnimationManager
{
public:
    static AnimationManager &instance();

    QPropertyAnimation* addAnimation(QObject *target, const QByteArray &propertyName, QObject *parent = 0);

private:
    AnimationManager();

    void updateSkipViewUpdate();
    void updateState(PropertyAnimationWrapper *animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    void updateCurrentValue(PropertyAnimationWrapper *animation, const QVariant &value);
    void removeAnimation(PropertyAnimationWrapper *animation);

private:
    Q_DISABLE_COPY(AnimationManager)

    friend class PropertyAnimationWrapper;

    struct CLAnimationEntry
    {
        CLAnimationEntry(CLAnimation *cl = 0)
            : clanimation(cl), started(false)
        {}

        CLAnimation *clanimation;
        bool started;
    };

    QHash<PropertyAnimationWrapper *, CLAnimationEntry> m_animations;
    QLinkedList<PropertyAnimationWrapper *> m_animationsOrder;
    bool m_skipViewUpdate;
};


class PropertyAnimationWrapper : public QPropertyAnimation
{
public:
    PropertyAnimationWrapper(QObject *target, const QByteArray &propertyName, QObject *parent = 0);
    ~PropertyAnimationWrapper();

    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    void updateCurrentValue(const QVariant &value);
};

#endif // _PROPERTY_ANIMATION_WRAPPER_H_
