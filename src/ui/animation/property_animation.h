/*
 *  property_animation.h
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#include "log.h"

#include "animation.h"

#ifndef _PROPERTY_ANIMATION_WRAPPER_H_
#define _PROPERTY_ANIMATION_WRAPPER_H_

class PropertyAnimationWrapper;

class AnimationManager
{
public:
	static AnimationManager& instance()
	{
		static AnimationManager instance;
		
		return instance;
	}
	
	QPropertyAnimation* addAnimation(QObject * target, const QByteArray & propertyName, QObject * parent = 0);
    
    void updateState (PropertyAnimationWrapper* animation, QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    void updateCurrentValue(PropertyAnimationWrapper* animation, const QVariant & value);
    
private:
    friend class PropertyAnimationWrapper;
    
	AnimationManager();
	
	QMap<PropertyAnimationWrapper*, CLAnimation*> m_animations;
    bool m_skipViewUpdate;
    
    PropertyAnimationWrapper* m_lastAnimation;
};

class PropertyAnimationWrapper : public QPropertyAnimation
{
public:
	PropertyAnimationWrapper(AnimationManager* manager, QObject * target, const QByteArray & propertyName, QObject * parent = 0);
	
	void updateState (QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    void updateCurrentValue ( const QVariant & value );
    
private:
	AnimationManager* m_manager; 
};

#endif // _PROPERTY_ANIMATION_WRAPPER_H_