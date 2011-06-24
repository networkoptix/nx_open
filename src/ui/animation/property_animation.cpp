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
    : m_skipViewUpdate(false),
      m_lastAnimation(0)
{
}

QPropertyAnimation* AnimationManager::addAnimation(QObject * target, const QByteArray & propertyName, QObject * parent)
{
    PropertyAnimationWrapper* animation = new PropertyAnimationWrapper(this, target, propertyName, parent);
 
    // For now there are problems on win32
#ifdef _WIN32
    return animation;
#endif

    CLAnimation* clanimation = dynamic_cast<CLAnimation*> (target);
    if (!clanimation)
        return animation;
    
    if (clanimation->isSkipViewUpdate())
        m_skipViewUpdate = true;
    
    m_animations.insert(animation, clanimation);
    m_lastAnimation = animation;
    
    return animation;
}

void AnimationManager::updateState (PropertyAnimationWrapper* animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    if (!m_animations.contains(animation))
        return;
    
    QMap<PropertyAnimationWrapper*, CLAnimation*>::const_iterator first = m_animations.begin();
    
    // Call updateView only once.
    if (m_lastAnimation != animation)
    {
        if (oldState == QAbstractAnimation::Running && newState == QAbstractAnimation::Stopped)
        {
            m_animations.remove(animation);
        }
        
        return;
    }
    
    CLAnimation* clanimation = m_animations[animation];
    
    if (oldState == QAbstractAnimation::Stopped && newState == QAbstractAnimation::Running)
    {
        clanimation->enableViewUpdateMode(false);
    } 
    else if (oldState == QAbstractAnimation::Running && newState == QAbstractAnimation::Stopped)
    {
        clanimation->enableViewUpdateMode(true);
        m_animations.remove(animation);
        
        foreach (CLAnimation* cl, m_animations)
        {
            if (cl->isSkipViewUpdate())
            {
                m_skipViewUpdate = true;
                return;
            }
        }
        m_skipViewUpdate = false;
    }
}

void AnimationManager::updateCurrentValue(PropertyAnimationWrapper* animation, const QVariant & value)
{
    if (!m_animations.contains(animation))
        return;
    
    // Call updateView only once
    if (m_lastAnimation != animation)
        return;
    
    CLAnimation* clanimation = m_animations[animation];
    
    if (!clanimation->isSkipViewUpdate())
    {
        clanimation->updateView();
    }
}

PropertyAnimationWrapper::PropertyAnimationWrapper(AnimationManager* manager, QObject * target, const QByteArray & propertyName, QObject * parent)
	: QPropertyAnimation(target, propertyName, parent),
	  m_manager(manager)	
{
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
