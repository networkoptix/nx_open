#include "scene_zoom.h"
#include <QScrollBar>
#include <math.h>
#include "../../base/log.h"
#include "../graphicsview.h"
#include "settings.h"


static const qreal min_zoom = 0.06;
static const qreal max_zoom = 4.0;
static const qreal def_zoom = 0.22;

static const qreal low_normal_qulity_zoom = 0.22;
static const qreal normal_hight_qulity_zoom = 0.30;
static const qreal hight_highest_qulity_zoom = 0.32;

CLSceneZoom::CLSceneZoom(GraphicsView* gview):
m_view(gview),
m_zoom(def_zoom+0.1),
m_targetzoom(0),
m_diff(0),
m_quality(CLStreamreader::CLSNormal)
{
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


void CLSceneZoom::zoom_abs(qreal z, int duration, int delay)
{
	m_targetzoom = z;

	zoom_helper(duration, delay);
}

void CLSceneZoom::zoom_minimum(int duration, int delay)
{
	zoom_abs(min_zoom , duration, delay);
}


void CLSceneZoom::zoom_delta(qreal delta, int duration, int delay )
{
	m_targetzoom += delta;
	zoom_helper(duration, delay);
}


void CLSceneZoom::valueChanged( qreal dpos )
{
	qreal start_point = m_targetzoom - m_diff;
	m_zoom = start_point + m_diff*dpos;

	//cl_log.log("m_zoom =", (float)m_zoom, cl_logDEBUG1);

	qreal scl = zoomToscale(m_zoom);

	QTransform tr;
	tr.scale(scl, scl);
	tr.rotate(global_rotation_angel, Qt::YAxis);
	m_view->setTransform(tr);
	m_view->addjustAllStaticItems();


	//=======================================
	bool zooming_out = m_targetzoom < getZoom();


	if (m_view->getSelectedItem() &&  zooming_out  && getZoom()<0.260) // if zooming out only
		m_view->setZeroSelection();

	if (zooming_out)
	{
		if (m_quality==CLStreamreader::CLSHighest && getZoom() <= hight_highest_qulity_zoom)
		{
			m_view->setAllItemsQuality(CLStreamreader::CLSHigh, false);
			m_quality = CLStreamreader::CLSHigh;
		}


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

		else if (m_quality==CLStreamreader::CLSHigh && getZoom() > hight_highest_qulity_zoom)
		{
			m_view->setAllItemsQuality(CLStreamreader::CLSHighest, true);
			m_quality = CLStreamreader::CLSHighest;
		}


	}

	//#include "camera\camera.h"

}

//============================================================

void CLSceneZoom::set_qulity_helper()
{
	
}


void CLSceneZoom::zoom_helper(int duration, int delay)
{
	if (m_targetzoom>max_zoom)
		m_targetzoom = max_zoom;

	if (m_targetzoom < min_zoom)
		m_targetzoom = min_zoom;

	if (duration==0)
	{
		m_zoom = m_targetzoom;
		m_diff = 0;
		valueChanged(1.0);
	}
	else
	{
		m_diff = m_targetzoom - m_zoom;
		start_helper(duration, delay);
	}

}
