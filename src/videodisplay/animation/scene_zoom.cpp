#include "scene_zoom.h"
#include <QGraphicsView>
#include <QScrollBar>
#include <math.h>
#include "../../base/log.h"

static const qreal zoom_factor = 100.0;
static const qreal min_zoom = -9000;
static const qreal max_zoom = 20000;

CLSceneZoom::CLSceneZoom(QGraphicsView* gview):
m_view(gview),
m_timeline(CLAnimationTimeLine::CLAnimationCurve::SLOW_END),
m_zoom(0),
m_targetzoom(0),
m_diff(0)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2000);
	m_timeline.setFrameRange(0, 20000);
	m_timeline.setUpdateInterval(17); // 60 fps

	connect(&m_timeline, SIGNAL(frameChanged(int)), this, SLOT(onNewPos(int)));

	

}

CLSceneZoom::~CLSceneZoom()
{
	stop();
}

void CLSceneZoom::stop()
{
	m_timeline.stop();
	//m_targetzoom = m_zoom;
	
}

void CLSceneZoom::zoom(int delta)
{
	

	if (abs(m_targetzoom - m_zoom)>5000)return;

	stop();

	m_targetzoom += delta*10;

	if (m_targetzoom>max_zoom)
		m_targetzoom = max_zoom;

	if (m_targetzoom < min_zoom)
		m_targetzoom = min_zoom;


	m_diff = m_targetzoom - m_zoom;

	
	//cl_log.log("m_targetzoom2=", m_targetzoom, cl_logDEBUG1);

	m_timeline.start();
	m_timeline.setCurrentTime(0);
}

int CLSceneZoom::getZoom() const
{
	return m_zoom;
}

void CLSceneZoom::onNewPos(int frame)
{
	qreal dpos = qreal(frame)/m_timeline.endFrame();
	int start_point = m_targetzoom - m_diff;

	m_zoom = start_point + m_diff*dpos;

	//cl_log.log("zoom=", m_zoom, cl_logDEBUG1);


	qreal scl;

	if (m_zoom<=0)
		scl = 1 + m_zoom/10000.0;
	else if (m_zoom>0)
		scl = 1 + pow(m_zoom,1.2)/10000;

		

	QTransform tr;
	tr.scale(scl, scl);
	m_view->setTransform(tr);

}

