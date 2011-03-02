#ifndef steady_mouse_animation_h2116
#define steady_mouse_animation_h2116

#include "abstract_animation.h"
class CLAbstractSceneItem;
class GraphicsView;

struct CLSteadyMouseAnimation : public QObject, public CLAbstractAnimation
{
	Q_OBJECT
public:
	explicit CLSteadyMouseAnimation(GraphicsView* view);
	virtual ~CLSteadyMouseAnimation();

    void onUserInput(bool go_unstedy = true);


    virtual void start();
	virtual void stopAnimation();
	virtual bool isRuning() const;
	virtual QObject* object();

	
	
private slots:
	void onTimer();
private:

	QTimer mTimer;
    GraphicsView* m_view;

    bool m_steadyMode;

    int m_counrer;
	
};

#endif //steady_mouse_animation_h2116