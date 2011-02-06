#include "group_animation.h"


CLParallelAnimationGroup::CLParallelAnimationGroup()
{
}


void CLParallelAnimationGroup::stopAnimation()
{
	QParallelAnimationGroup::stop();
}

bool CLParallelAnimationGroup::isRuning() const 
{
	return true;
}


QObject* CLParallelAnimationGroup::object()
{
	return this;
}

