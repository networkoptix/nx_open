#include "ui\videoitem\abstract_scene_item.h"
#include "ui\graphicsview.h"
#include "steady_mouse_animation.h"

CLSteadyMouseAnimation::CLSteadyMouseAnimation(GraphicsView* view):
m_view(view),
m_counrer(0),
m_steadyMode(false)
{
	connect(&mTimer, SIGNAL(timeout()), this , SLOT(onTimer()));
	mTimer.setInterval(1000); // 60 fps   
}


CLSteadyMouseAnimation::~CLSteadyMouseAnimation()
{
	mTimer.stop();
}


void CLSteadyMouseAnimation::start()
{
    m_counrer = 0;
    mTimer.start();
}

void CLSteadyMouseAnimation::stopAnimation()
{
	mTimer.stop();
	m_counrer = 0;
}

bool CLSteadyMouseAnimation::isRuning() const
{
	return mTimer.isActive();
}

QObject* CLSteadyMouseAnimation::object()
{
	return this;
}


void CLSteadyMouseAnimation::onTimer()
{

    if (m_steadyMode)
        return;

    ++m_counrer;

    if (m_counrer > 4)
    {
        m_steadyMode = true;
        m_view->goToSteadyMode(true);
    }
}

void CLSteadyMouseAnimation::onUserInput(bool go_unstedy)
{
    m_counrer = 0;


    if (m_steadyMode && go_unstedy)
    {
        m_view->goToSteadyMode(false);
    }

    m_steadyMode = false;
}