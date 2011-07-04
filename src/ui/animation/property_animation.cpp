/*
 *  property_animation.cpp
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#include "property_animation.h"

AnimationManager::AnimationManager()
    : m_skipViewUpdate(false)
{
}

QPropertyAnimation* AnimationManager::addAnimation(QObject * target, const QByteArray & propertyName, QObject * parent)
{
    PropertyAnimationWrapper* animation = new PropertyAnimationWrapper(this, target, propertyName, parent);
 
    CLAnimation* clanimation = dynamic_cast<CLAnimation*> (target);
    if (!clanimation)
        return animation;

    m_animations.insert(animation, clanimation);
    m_animationsOrder.append(animation);

    return animation;
}

void AnimationManager::removeAnimation(PropertyAnimationWrapper* animation)
{
    if (!m_animations.contains(animation))
        return;
    
    CLAnimationEntry& entry = m_animations[animation];

    if (m_animations.size() == 1)
    {
        entry.clanimation->enableViewUpdateMode(true);
    }
    
    m_animations.remove(animation);
    m_animationsOrder.removeOne(animation);

    
    updateSkipViewUpdate();
}

void AnimationManager::updateState (PropertyAnimationWrapper* animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    if (!m_animations.contains(animation))
        return;
    
    CLAnimationEntry& entry = m_animations[animation];
    
    if (oldState == QAbstractAnimation::Stopped && newState == QAbstractAnimation::Running)
    {
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
    foreach (const CLAnimationEntry& entry, m_animations)
    {
        if (entry.started && entry.clanimation->isSkipViewUpdate())
        {
            m_skipViewUpdate = true;
            return;
        }
    }
    m_skipViewUpdate = false;
}

void AnimationManager::updateCurrentValue(PropertyAnimationWrapper* animation, const QVariant& /*value*/)
{
    if (!m_animations.contains(animation))
        return;

    // Call updateView only once
    if (m_animationsOrder.back() != animation)
        return;

    CLAnimationEntry& entry = m_animations[animation];
    if (!m_skipViewUpdate)
    {
        entry.clanimation->updateView();
    }
}

PropertyAnimationWrapper::PropertyAnimationWrapper(AnimationManager* manager, QObject * target, const QByteArray & propertyName, QObject * parent)
	: QPropertyAnimation(target, propertyName, parent),
	  m_manager(manager)	
{
}

PropertyAnimationWrapper::~PropertyAnimationWrapper()
{
    m_manager->removeAnimation(this);
}

void PropertyAnimationWrapper::updateState (QAbstractAnimation::State newState, QAbstractAnimation::State oldState) 
{
    m_manager->updateState(this, newState, oldState);
    
	QPropertyAnimation::updateState(newState, oldState);
}

void PropertyAnimationWrapper::updateCurrentValue (const QVariant & value)
{
    QPropertyAnimation::updateCurrentValue(value);
    
    m_manager->updateCurrentValue(this, value);
}
