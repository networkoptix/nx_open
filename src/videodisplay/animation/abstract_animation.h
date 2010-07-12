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
		virtual void valueChanged ( qreal pos ) = 0;
		virtual void onFinished();
protected:
		void start_helper(int duration);
protected:
	CLAnimationTimeLine m_timeline;
	int m_defaultduration;

protected:
	template< typename T>
	struct CLAnimationUnit
	{
		CLAnimationUnit(T val)
		{
			diff = T();
			curent = val;
			start_point = val;
			target = val;
		}
		T curent;
		T start_point;
		T diff;
		T target;
	};


};

#endif //abstactanimation_h2344