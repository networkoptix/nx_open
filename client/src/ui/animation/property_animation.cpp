/*
 *  property_animation.cpp
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#include "property_animation.h"

#include <QtCore/QHash>
#include <QtCore/QLinkedList>

#include "animation.h"

class PropertyAnimationWrapper;

class AnimationManagerPrivate
{
public:
    AnimationManagerPrivate() : m_skipViewUpdate(false) {}

    QPropertyAnimation *addAnimation(QObject *target, const QByteArray &propertyName, QObject *parent = 0);

    void updateSkipViewUpdate();
    void updateState(PropertyAnimationWrapper *animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    void updateCurrentValue(PropertyAnimationWrapper *animation, const QVariant &value);
    void removeAnimation(PropertyAnimationWrapper *animation);

private:
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

Q_GLOBAL_STATIC(AnimationManagerPrivate, animationManager)


class PropertyAnimationWrapper : public QPropertyAnimation
{
public:
    PropertyAnimationWrapper(QObject *target, const QByteArray &propertyName, QObject *parent = 0)
        : QPropertyAnimation(target, propertyName, parent)
    {}
    ~PropertyAnimationWrapper()
    { animationManager()->removeAnimation(this); }

    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        animationManager()->updateState(this, newState, oldState);
        QPropertyAnimation::updateState(newState, oldState);
    }

    void updateCurrentValue(const QVariant &value)
    {
        QPropertyAnimation::updateCurrentValue(value);
        animationManager()->updateCurrentValue(this, value);
    }
};


QPropertyAnimation *AnimationManagerPrivate::addAnimation(QObject *target, const QByteArray &propertyName, QObject *parent)
{
    PropertyAnimationWrapper *animation = new PropertyAnimationWrapper(target, propertyName, parent);

    // ### avoid using RTTI
    CLAnimation *clanimation = dynamic_cast<CLAnimation *>(target);
    if (clanimation)
    {
        m_animations.insert(animation, clanimation);
        m_animationsOrder.append(animation);
    }

    return animation;
}

void AnimationManagerPrivate::removeAnimation(PropertyAnimationWrapper *animation)
{
    if (!m_animations.contains(animation))
        return;

    if (m_animations.size() == 1)
        m_animations[animation].clanimation->enableViewUpdateMode(true);

    m_animations.remove(animation);
    m_animationsOrder.removeOne(animation);

    updateSkipViewUpdate();
}

void AnimationManagerPrivate::updateState(PropertyAnimationWrapper *animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    if (!m_animations.contains(animation))
        return;

    if (oldState == QAbstractAnimation::Stopped && newState == QAbstractAnimation::Running)
    {
        CLAnimationEntry& entry = m_animations[animation];
        entry.clanimation->enableViewUpdateMode(false);
        entry.started = true;

        updateSkipViewUpdate();
    }
    else if (oldState == QAbstractAnimation::Running && newState == QAbstractAnimation::Stopped)
    {
        removeAnimation(animation);
    }
}

void AnimationManagerPrivate::updateSkipViewUpdate()
{
    // Check if there any animation which forces us to skip update remains
    foreach (const CLAnimationEntry &entry, m_animations)
    {
        if (entry.started && entry.clanimation->isSkipViewUpdate())
        {
            m_skipViewUpdate = true;
            return;
        }
    }
    m_skipViewUpdate = false;
}

void AnimationManagerPrivate::updateCurrentValue(PropertyAnimationWrapper *animation, const QVariant &value)
{
    Q_UNUSED(value);

    if (!m_animations.contains(animation))
        return;

    // Call updateView only once
    if (m_animationsOrder.back() != animation)
        return;

    CLAnimationEntry& entry = m_animations[animation];
    if (!m_skipViewUpdate)
        entry.clanimation->updateView();
}


QPropertyAnimation *AnimationManager::addAnimation(QObject *target, const QByteArray &propertyName, QObject *parent)
{
    return animationManager()->addAnimation(target, propertyName, parent);
}
