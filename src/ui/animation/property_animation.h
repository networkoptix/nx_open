/*
 *  property_animation.h
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#include "base/log.h"

#include "animation.h"

#ifndef _PROPERTY_ANIMATION_WRAPPER_H_
#define _PROPERTY_ANIMATION_WRAPPER_H_

class PropertyAnimationWrapper;

/**
  * @class AnimationManager
  *
  * Manage concurrent animation to not allow multiple repaints
  */
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
	AnimationManager();
    
    void updateSkipViewUpdate();
    void removeAnimation(PropertyAnimationWrapper* animation);
    
private:
    friend class PropertyAnimationWrapper;
    
    struct CLAnimationEntry
    {
        CLAnimationEntry(CLAnimation* cl = 0)
        : clanimation(cl),
          started(false)
        {
        }
        
        CLAnimation* clanimation;
        bool started;
    };
    
	QMap<PropertyAnimationWrapper*, CLAnimationEntry> m_animations;
    QLinkedList<PropertyAnimationWrapper*> m_animationsOrder;
    bool m_skipViewUpdate;
};

class PropertyAnimationWrapper : public QPropertyAnimation
{
public:
	PropertyAnimationWrapper(AnimationManager* manager, QObject * target, const QByteArray & propertyName, QObject * parent = 0);
    ~PropertyAnimationWrapper();
	
	void updateState (QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    void updateCurrentValue (const QVariant & value);
    
private:
	AnimationManager* m_manager; 
};

#endif // _PROPERTY_ANIMATION_WRAPPER_H_