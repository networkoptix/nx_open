/*
 *  property_animation.cpp
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#include "property_animation.h"

AnimationManager &AnimationManager::instance()
{
    // ### isn't thread-safe
    static AnimationManager instance;

    return instance;
}

AnimationManager::AnimationManager()
    : m_skipViewUpdate(false)
{
}

QPropertyAnimation* AnimationManager::addAnimation(QObject *target, const QByteArray &propertyName, QObject *parent)
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

void AnimationManager::removeAnimation(PropertyAnimationWrapper *animation)
{
    if (!m_animations.contains(animation))
        return;

    if (m_animations.size() == 1)
        m_animations[animation].clanimation->enableViewUpdateMode(true);

    m_animations.remove(animation);
    m_animationsOrder.removeOne(animation);

    updateSkipViewUpdate();
}

void AnimationManager::updateState(PropertyAnimationWrapper *animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
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

void AnimationManager::updateSkipViewUpdate()
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

void AnimationManager::updateCurrentValue(PropertyAnimationWrapper *animation, const QVariant &value)
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


PropertyAnimationWrapper::PropertyAnimationWrapper(QObject *target, const QByteArray &propertyName, QObject *parent)
    : QPropertyAnimation(target, propertyName, parent)
{
}

PropertyAnimationWrapper::~PropertyAnimationWrapper()
{
    AnimationManager::instance().removeAnimation(this);
}

void PropertyAnimationWrapper::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    AnimationManager::instance().updateState(this, newState, oldState);

    QPropertyAnimation::updateState(newState, oldState);
}

void PropertyAnimationWrapper::updateCurrentValue(const QVariant &value)
{
    QPropertyAnimation::updateCurrentValue(value);

    AnimationManager::instance().updateCurrentValue(this, value);
}
