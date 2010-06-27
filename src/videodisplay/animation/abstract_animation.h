#ifndef abstactanimation_h2344
#define abstactanimation_h2344

#include "animation_timeline.h"



class CLAbstractAnimation: public QObject
{
	Q_OBJECT
public:
	CLAbstractAnimation();
	virtual ~CLAbstractAnimation();

	virtual void stop();
	bool isRuning() const;

protected slots:
		virtual void onNewFrame(int frame)=0;
		virtual void onFinished();
protected:
		void start_helper(int duration);
protected:
	CLAnimationTimeLine m_timeline;
	int m_defaultduration;

protected:
	struct CLAnimationUnit
	{
		CLAnimationUnit(qreal val)
		{
			diff = 0;
			curent = val;
			start_point = val;
		}
		qreal curent;
		qreal start_point;
		qreal diff;
	};


};

#endif //abstactanimation_h2344