#include "graphicsview.h"
#include <QWheelEvent>
#include <QScrollBar>
#include "./base/log.h"
#include <math.h>





//==============================================================================

GraphicsView::GraphicsView():
QGraphicsView(),
m_xRotate(0), m_yRotate(0),
m_movement(this),
m_scenezoom(this),
m_handScrolling(false)
{

}

GraphicsView::~GraphicsView()
{
	
}


void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	int numDegrees = e->delta() / 8;
	m_scenezoom.zoom(numDegrees*8);
}

void GraphicsView::zoom(int z)
{
	m_scenezoom.zoom(z);
}

void GraphicsView::updateTransform()
{
	QTransform tr;


	QPoint center = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

	tr.translate(center.x(), center.y());
	tr.rotate(m_yRotate/1.0, Qt::YAxis);
	tr.rotate(m_xRotate/1.0, Qt::XAxis);
	tr.translate(-center.x(), -center.y());

	setTransform(tr);

}


void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	m_scenezoom.stop();
	m_movement.stop();

	if (event->button() == Qt::LeftButton) 
	{
		/*
		QGraphicsItem *item = itemAt(event->pos());

		if (item)
		{
			//QPointF point = item->pos() + item->boundingRect().center();
			QPointF point = item->mapToScene(item->boundingRect().center());
			m_movement.move(point);
		}
		else
			m_movement.move(mapToScene(event->pos()));
			/**/

		// Left-button press in scroll hand mode initiates hand scrolling.


		m_mousestate.clearEvents();
		m_mousestate.mouseMoveEventHandler(event);
        event->accept();
        m_handScrolling = true;
		viewport()->setCursor(Qt::ClosedHandCursor);
	}
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
	if (m_handScrolling) 
	{
		QPoint delta = event->pos() - m_mousestate.getLastEventPoint();


		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();
		hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
		vBar->setValue(vBar->value() - delta.y());
		

		m_mousestate.mouseMoveEventHandler(event);

		
	}	
	QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent ( QMouseEvent * event)
{
	
	viewport()->setCursor(Qt::OpenHandCursor);
    m_handScrolling = false;

	m_mousestate.mouseMoveEventHandler(event);

	qreal mouse_spped, mouse_spped_h, mouse_spped_v;

	m_mousestate.getMouseSpeed(mouse_spped, mouse_spped_h, mouse_spped_v);

	cl_log.log("====== ", cl_logDEBUG1);
	cl_log.log("mouse_spped = ", (int)mouse_spped, cl_logDEBUG1);
	cl_log.log("mouse_spped_h = ", (int)mouse_spped_h, cl_logDEBUG1);
	cl_log.log("mouse_spped_v = ", (int)mouse_spped_v, cl_logDEBUG1);

	if (mouse_spped<100)
		return;

	bool sdx = (mouse_spped_h<0);
	bool sdy = (mouse_spped_v<0);

	int dx = pow( abs(mouse_spped_h), 1.9 )/2000;
	int dy = pow( abs(mouse_spped_v), 1.9 )/2000;;

	if (sdx) dx =-dx;
	if (sdy) dy =-dy;

	cl_log.log("dx = ", dx, cl_logDEBUG1);
	cl_log.log("dy = ", dy, cl_logDEBUG1);

	m_movement.move(-dx,-dy);
}


void GraphicsView::keyPressEvent( QKeyEvent * e )
{
	QTransform tr = transform();
	switch (e->key()) {
		case Qt::Key_W:
			centerOn(QPoint(0,0));
			break;
		case Qt::Key_Left:
			m_yRotate -= 1;
			updateTransform();
			break;
		case Qt::Key_Right:
			m_yRotate += 1;
			updateTransform();
			break;
		case Qt::Key_Up:
			m_xRotate += 1;
			updateTransform();
			break;
		case Qt::Key_Down:
			m_xRotate -= 1;
			updateTransform();
			break;
	}
}
