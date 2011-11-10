#include "ui/videoitem/abstract_scene_item.h"
#include "ui/graphicsview.h"
#include "steady_mouse_animation.h"

CLSteadyMouseAnimation::CLSteadyMouseAnimation(GraphicsView* view):
m_view(view),
m_steadyMode(false),
m_counrer(0)
{
	connect(&m_timer, SIGNAL(timeout()), this , SLOT(onTimer()));
	m_timer.setInterval(500);  
}

CLSteadyMouseAnimation::~CLSteadyMouseAnimation()
{
	m_timer.stop();
}

void CLSteadyMouseAnimation::start()
{
    m_counrer = 0;
    m_timer.start();
}

void CLSteadyMouseAnimation::stopAnimation()
{
	m_timer.stop();
	m_counrer = 0;
}

bool CLSteadyMouseAnimation::isRuning() const
{
	return m_timer.isActive();
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

    if (m_counrer > 3)
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
