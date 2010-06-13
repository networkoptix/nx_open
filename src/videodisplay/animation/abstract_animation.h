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
	void setDuration(int ms);

	int getDefaultDuration() const;
	void setDefaultDuration(int val);
	void restoreDefaultDuration();

	bool isRuning() const;

protected slots:
		virtual void onNewFrame(int frame)=0;
		virtual void onFinished();
	
protected:
	CLAnimationTimeLine m_timeline;

	int m_defaultduration;


};

#endif //abstactanimation_h2344