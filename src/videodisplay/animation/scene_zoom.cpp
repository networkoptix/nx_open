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
m_zoom(0),
m_targetzoom(0),
m_diff(0)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2500);
	m_timeline.setFrameRange(0, 20000);
}

CLSceneZoom::~CLSceneZoom()
{
	stop();
}

void CLSceneZoom::zoom(int delta)
{

	if (abs(m_targetzoom - m_zoom)>3000)return;

	m_targetzoom += delta*10;

	if (m_targetzoom>max_zoom)
		m_targetzoom = max_zoom;

	if (m_targetzoom < min_zoom)
		m_targetzoom = min_zoom;


	m_diff = m_targetzoom - m_zoom;
	
	//cl_log.log("m_diff =", m_diff , cl_logDEBUG1);

	if (!isRuning())
		m_timeline.start();

	m_timeline.setCurrentTime(0);
}

int CLSceneZoom::getZoom() const
{
	return m_zoom;
}

void CLSceneZoom::onNewFrame(int frame)
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

