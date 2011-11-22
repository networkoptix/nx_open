#include "animation.h"
#include "ui/graphicsview.h"


CLAnimation::CLAnimation(GraphicsView* gview):
    m_view(gview),
    m_animationMutex(QMutex::Recursive),
    m_animation(0),
    m_skipViewUpdate(false)
{
	//m_delay_timer.setSingleShot(true);
	//connect(&m_timeline, SIGNAL(valueChanged(qreal)), this, SLOT(valueChanged(qreal)));
	//connect(&m_timeline, SIGNAL(finished()), this, SLOT(onFinished()));
}
    
CLAnimation::~CLAnimation()
{
	stopAnimation();
}

QObject* CLAnimation::object() 
{
	return this;
}

bool CLAnimation::isRuning() const
{
    QMutexLocker _locker(&m_animationMutex);
    
	return (m_animation && (m_animation->state() & QAbstractAnimation::Running));
}


void CLAnimation::stopAnimation()
{
    QMutexLocker _locker(&m_animationMutex);

    if (!m_animation)
        return;

    disconnect(m_animation, SIGNAL(finished()), this, SLOT(onFinished()));

    m_animation->stop();
    delete m_animation;

    m_animation = 0;

    m_delay_timer.stop();
}

void CLAnimation::Start()
{
    QMutexLocker _locker(&m_animationMutex);

	if (m_animation)
    {
        connect(m_animation, SIGNAL(finished()), this, SLOT(onFinished()));
        m_animation->start();
    }
}

void CLAnimation::start_helper(int /*duration*/, int delay)
{
	if (!isRuning())
	{

		m_delay_timer.stop(); // just in case if it's still running

		if (delay==0)
			Start();
		else
		{
			//QTimer::singleShot(delay, this, SLOT(Start()));
			connect(&m_delay_timer, SIGNAL(timeout()), this, SLOT(Start()));
			m_delay_timer.start(delay);
		}
	}
}

void CLAnimation::onFinished()
{
	emit finished();
    stopAnimation();
}

void CLAnimation::enableViewUpdateMode(bool enable)
{
    if (!m_view)
    {
        // problem 1
        return;
    }
    
    m_view->setViewportUpdateMode(enable ? QGraphicsView::MinimalViewportUpdate : QGraphicsView::NoViewportUpdate);
}

void CLAnimation::updateView()
{
    if (!m_view)
    {
        // problem 1
        return;
    }
    
    m_view->viewport()->update(); // problem 2
}

bool CLAnimation::isSkipViewUpdate() const
{
    return m_skipViewUpdate;
}
