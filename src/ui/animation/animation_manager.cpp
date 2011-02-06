#include "animation_manager.h"
#include "abstract_animation.h"


CLAnimationManager::CLAnimationManager()
{

}

CLAnimationManager::~CLAnimationManager()
{
	stopAllAnimations();
}

void CLAnimationManager::registerAnimation(CLAbstractAnimation* anim)
{
	m_anim_list.push_back(anim);
}


void CLAnimationManager::stopAllAnimations()
{
	// if stop is called => onAnimationFinished would not be called; 
	// and vice versa 

	for (QList<CLAbstractAnimation*>::iterator it = m_anim_list.begin(); it!=m_anim_list.end();)
	{
		CLAbstractAnimation* anim = *it;
		anim->stopAnimation();
		if (anim->deleteAfterFinished())
		{
			delete anim;
			it = m_anim_list.erase(it);
		}
		else
		{
			++it;
		}

	}
	
}
