#include "animated_show.h"
#include "ui/videoitem/abstract_scene_item.h"
#include "base/rand.h"
#include "ui/graphicsview.h"

Show_helper::Show_helper(GraphicsView* view):
m_view(view),
m_showTime(false),
m_counrer(0),
m_value(0)
{
	connect(&mTimer, SIGNAL(timeout()), this , SLOT(onTimer()));
	mTimer.setInterval(1000/60); // 60 fps   
}

Show_helper::~Show_helper()
{
	mTimer.stop();
}

void Show_helper::setCenterPoint(QPointF center)
{
	m_center = center;
}

QPointF Show_helper::getCenter() const
{
	return m_center;
}

void Show_helper::setRadius(int radius)
{
	m_radius = radius;
}

int Show_helper::getRadius() const
{
	return m_radius;
}

void Show_helper::setItems(QList<CLAbstractSceneItem*>* items)
{
	m_items = items;
}

void Show_helper::setShowTime(bool showtime)
{
	m_showTime = showtime;
}

bool Show_helper::isShowTime() const
{
	return m_showTime;
}

void Show_helper::start()
{
	m_value = 0;
	setShowTime(true); // just in case
	mTimer.start();
}

void Show_helper::stopAnimation()
{
	mTimer.stop();
	m_counrer = 0;
}

bool Show_helper::isRuning() const
{
	return mTimer.isActive();
}

QObject* Show_helper::object()
{
	return this;
}

void Show_helper::onTimer()
{
	m_value++;

	qreal total = m_items->size();

	int i = 0;

	foreach (CLAbstractSceneItem* item, *m_items)
	{
		QPointF pos ( m_center.x() + cos((i / total) * 6.28 + m_value/100.0) * m_radius , m_center.y() + sin((i / total) * 6.28 + m_value/100.0) * m_radius ) ;

		item->setPos(pos);

		item->setRotation(item->getRotation() - 4.0/cl_get_random_val(7,10));

		++i;
	}

	// scene rotation===========
	{
		const int max_val = 800;

		if (m_value<max_val/2)
			m_view->updateTransform(-0.001);
		else
		{
			int mod = (m_value - max_val/2)%max_val;
			if (mod==0)	m_positive_dir = !m_positive_dir;

			if (m_positive_dir)
				m_view->updateTransform(0.001);
			else
				m_view->updateTransform(-0.001);
		}
	}

}
