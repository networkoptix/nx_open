#include "scene_zoom.h"
#include <QScrollBar>
#include <math.h>
#include "../../base/log.h"
#include "../graphicsview.h"


static const qreal min_zoom = 0.16;
static const qreal max_zoom = 4.0;
static const qreal def_zoom = 0.22;

static const qreal low_normal_qulity_zoom = 0.22;
static const qreal normal_hight_qulity_zoom = 0.30;

CLSceneZoom::CLSceneZoom(GraphicsView* gview):
m_view(gview),
m_zoom(def_zoom+0.1),
m_targetzoom(0),
m_diff(0),
m_quality(CLStreamreader::CLSNormal)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setDuration(2500);
	setDefaultDuration(2500);

}

CLSceneZoom::~CLSceneZoom()
{
	stop();
}

qreal CLSceneZoom::zoomToscale(qreal zoom) const
{
	return zoom*zoom;
}

qreal CLSceneZoom::scaleTozoom(qreal scale) const
{
	return sqrt(scale);
}

qreal CLSceneZoom::getZoom() const
{
	return m_zoom;
}


void CLSceneZoom::zoom_abs(qreal z)
{
	m_targetzoom = z;

	zoom_helper();
}

void CLSceneZoom::zoom_default()
{
	zoom_abs(def_zoom);
}

void CLSceneZoom::zoom_delta(qreal delta)
{

	//if (abs(m_targetzoom - m_zoom)>1300*4)return;

	m_targetzoom += delta;

	zoom_helper();
}

void CLSceneZoom::onNewFrame(int frame)
{
	qreal dpos = qreal(frame)/m_timeline.endFrame();
	qreal start_point = m_targetzoom - m_diff;

	m_zoom = start_point + m_diff*dpos;

	//cl_log.log("m_zoom =", (float)m_zoom, cl_logDEBUG1);

	qreal scl = zoomToscale(m_zoom);

	QTransform tr;
	tr.scale(scl, scl);
	m_view->setTransform(tr);


	//=======================================
	bool zooming_out = m_targetzoom < getZoom();


	if (m_view->getSelectedWnd() &&  zooming_out  && getZoom()<0.280) // if zooming out only
		m_view->setZeroSelection();

	if (zooming_out)
	{
		if (m_quality==CLStreamreader::CLSHigh && getZoom() <= normal_hight_qulity_zoom)
		{
			m_view->setAllItemsQuality(CLStreamreader::CLSNormal, false);
			m_quality = CLStreamreader::CLSNormal;
		}

		else if (m_quality==CLStreamreader::CLSNormal&& getZoom() <= low_normal_qulity_zoom)
		{
			m_view->setAllItemsQuality(CLStreamreader::CLSLow, false);
			m_quality = CLStreamreader::CLSLow;
		}
	}
	else// zoomin in
	{
		if (m_quality==CLStreamreader::CLSLow && getZoom() > low_normal_qulity_zoom)
		{
			m_view->setAllItemsQuality(CLStreamreader::CLSNormal, true);
			m_quality = CLStreamreader::CLSNormal;
		}

		else if (m_quality==CLStreamreader::CLSNormal && getZoom() > normal_hight_qulity_zoom)
		{
			m_view->setAllItemsQuality(CLStreamreader::CLSHigh, true);
			m_quality = CLStreamreader::CLSHigh;
		}

	}

	//#include "camera\camera.h"

}

//============================================================

void CLSceneZoom::set_qulity_helper()
{
	
}


void CLSceneZoom::zoom_helper()
{
	if (m_targetzoom>max_zoom)
		m_targetzoom = max_zoom;

	if (m_targetzoom < min_zoom)
		m_targetzoom = min_zoom;


	m_diff = m_targetzoom - m_zoom;

	//cl_log.log("m_diff =", m_diff , cl_logDEBUG1);

	if (!isRuning())
	{
		m_timeline.start();
		m_timeline.start();
	}

	m_timeline.setCurrentTime(0);

}
