#ifndef animation_h2344
#define animation_h2344

#include "animation_timeline.h"
#include "abstract_animation.h"

class CLAnimation: public QObject, public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLAnimation();
	virtual ~CLAnimation();

	virtual void stopAnimation();
	bool isRuning() const;

	QObject* object() ;
signals:
	void finished();
protected slots:
		virtual void valueChanged ( qreal pos ) = 0;
		virtual void onFinished();
		virtual void Start();
protected:
		void start_helper(int duration, int delay);
protected:
	CLAnimationTimeLine m_timeline;
	int m_defaultduration;
	QTimer m_delay_timer;

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

#endif //animation_h2344