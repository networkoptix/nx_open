#ifndef animation_h2344
#define animation_h2344

#include "animation_timeline.h"
#include "abstract_animation.h"

class GraphicsView;

class CLAnimation: public QObject, public CLAbstractAnimation
{
	Q_OBJECT
    
public:
	CLAnimation(GraphicsView* gview);
	virtual ~CLAnimation();

	virtual void stopAnimation();
	bool isRuning() const;

	QObject* object();
    
    void enableViewUpdateMode(bool value);
    void updateView();
    
    bool isSkipViewUpdate() const;

signals:
	void finished();
    
protected slots:
	virtual void onFinished();
	virtual void Start();
    
protected:
	void start_helper(int duration, int delay);
    
protected:
	QTimer m_delay_timer;
    
    GraphicsView* m_view;

    mutable QMutex m_animationMutex;
    QAbstractAnimation *m_animation;
    
    bool m_skipViewUpdate;
};

#endif //animation_h2344
