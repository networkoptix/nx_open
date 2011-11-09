#ifndef group_animation_h2219
#define group_animation_h2219

#include "abstract_animation.h"

class CLParallelAnimationGroup : public QParallelAnimationGroup, public CLAbstractAnimation
{
public:
	CLParallelAnimationGroup();

	virtual void stopAnimation();
	virtual bool isRuning() const;
	virtual QObject* object();
};

#endif //group_animation_h2219
