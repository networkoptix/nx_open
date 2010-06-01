#ifndef abstactanimation_h2344
#define abstactanimation_h2344

#include "animation_timeline.h"



class CLAbstractAnimation: public QObject
{
	Q_OBJECT
public:
	CLAbstractAnimation();
	virtual ~CLAbstractAnimation();

	void stop();

protected slots:
		virtual void onNewFrame(int frame)=0;
	
protected:
	CLAnimationTimeLine m_timeline;


};

#endif //abstactanimation_h2344