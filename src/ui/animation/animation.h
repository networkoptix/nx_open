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
		virtual void onFinished();
		virtual void Start();
protected:
		void start_helper(int duration, int delay);
protected:
	QTimer m_delay_timer;
    QAbstractAnimation *m_animation;


};

#endif //animation_h2344
