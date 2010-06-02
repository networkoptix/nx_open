#include "graphicsview.h"
#include <QWheelEvent>
#include <math.h>
#include <QGraphicsProxyWidget>
#include <QPushButton>

#include <qDebug>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QScrollBar>
#include <QTimeLine>
#include <QQueue>
#include <QTime>

struct GraphicsView::GraphicsViewPriv
{
	GraphicsViewPriv(QGraphicsView* _q): 
		q(_q),
		handScrolling(false), 
		handScrollMotions(0),
		useLastMouseEvent(false),
		mousePressButton(Qt::NoButton),
		lastMouseEvent(QEvent::None, QPoint(), Qt::NoButton, 0, 0),
		mouseSpeed(0.0),
		timeline(0)
	{
	}

	void storeMouseEvent(QMouseEvent *event)
	{
		useLastMouseEvent = true;
		lastMouseEvent = QMouseEvent(QEvent::MouseMove, event->pos(), event->globalPos(),
									 event->button(), event->buttons(), event->modifiers());
	}


	void mouseMoveEventHandler(QMouseEvent *event)
	{
		storeMouseEvent(event);
		lastMouseEvent.setAccepted(false);

		while (mouseTrackQueue.size() > 10)
			mouseTrackQueue.dequeue();
		mouseTrackQueue.enqueue(QPair<QPoint, QTime> (event->pos(), QTime::currentTime()));
	}

	qreal calcPointRange(const QPoint& a, const QPoint& b) {
		int dx = a.x() - b.x();
		int dy = a.y() - b.y();
		return sqrt((double)dx*dx + (double)dy*dy);
	}

	void normilizeValue(qreal& moveDx, qreal& moveDy)
	{
		bool signDx = moveDx < 0;
		bool signDy = moveDy < 0;
		moveDx = fabs(moveDx);
		moveDy = fabs(moveDy);
		qreal maxVal = qMax(moveDx, moveDy);
		moveDx /= maxVal;
		moveDy /= maxVal;
		if (signDx)
			moveDx = -moveDx;
		if (signDy)
			moveDy = -moveDy;
	}

	qreal calcMouseSpeed() 
	{
		moveDx = moveDy = 0;
		mouseSpeed = 0;
		if (mouseTrackQueue.isEmpty())
			return 0;
		QTime firstTime = mouseTrackQueue.head().second;
		QPoint firstPoint = mouseTrackQueue.head().first;
		while (mouseTrackQueue.size() > 1) 
			mouseTrackQueue.dequeue();
		QTime lastTime = mouseTrackQueue.head().second;
		QPoint lastPoint = mouseTrackQueue.head().first;
		moveDx = lastPoint.x() - firstPoint.x();
		moveDy = lastPoint.y() - firstPoint.y();
		normilizeValue(moveDx, moveDy);
		qreal range = calcPointRange(lastPoint, firstPoint);
		mouseSpeed = range / ((lastTime.msec() - firstTime.msec()) / 1000.0);

		// Roma, "not all control paths return a value"
	}

	QGraphicsView* q;
	bool handScrolling;
	bool useLastMouseEvent;
	int handScrollMotions;
	QPoint mousePressScenePoint;
	QPoint mousePressScreenPoint;
	QPoint lastMouseMoveScenePoint;
	QPoint lastMouseMoveScreenPoint;
	QPoint mousePressPos;
	QPoint startAnimationCoordinate;
	QMouseEvent lastMouseEvent;
	Qt::MouseButton mousePressButton;
	QTimeLine* timeline;
	double mouseSpeed;
	qreal moveDx;
	qreal moveDy;
	QQueue<QPair<QPoint, QTime> > mouseTrackQueue;
};
//==============================================================================

GraphicsView::GraphicsView():
QGraphicsView(),
m_zoom(0),
m_xRotate(0), m_yRotate(0),
m_dragStarted(false),
m_movement(this),
m_scenezoom(this)

{
	d = new GraphicsViewPriv(this);
}

GraphicsView::~GraphicsView()
{
	delete d;
}

int GraphicsView::zoom() const
{
	return m_zoom;
}

void GraphicsView::wheelEvent ( QWheelEvent * e )
{
	int numDegrees = e->delta() / 8;
	m_scenezoom.zoom(numDegrees*8);
}

void GraphicsView::updateTransform()
{
	/**/
	QTransform tr;


	float f;
	if (m_zoom>=0)
		f = m_zoom*m_zoom/10000.0 + 1;
	else
		f = m_zoom/200.0 + 1;

	QPoint center = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

	tr.translate(center.x(), center.y());
	tr.scale(f, f);
	tr.rotate(m_yRotate/1.0, Qt::YAxis);
	tr.rotate(m_xRotate/1.0, Qt::XAxis);
	tr.translate(-center.x(), -center.y());

	setTransform(tr);
	/**/


	/*
	QTransform tr;
	tr.translate(10,10);
	setTransform(tr,true);
	/**/
}


void GraphicsView::mousePressEvent ( QMouseEvent * event)
{
	if (d->timeline) {
		d->timeline->stop();
		d->timeline->deleteLater();
		d->timeline = 0;
	}
	if (event->button() == Qt::LeftButton) 
	{

		QGraphicsItem *item = itemAt(event->pos());

		if (item)
		{
			//QPointF point = item->pos() + item->boundingRect().center();
			QPointF point = item->mapToScene(item->boundingRect().center());
			m_movement.move(point);
		}
		else
			m_movement.move(mapToScene(event->pos()));

		// Left-button press in scroll hand mode initiates hand scrolling.
		d->mouseTrackQueue.clear();
		d->mouseMoveEventHandler(event);
        event->accept();
        d->handScrolling = true;
		d->mousePressPos = event->pos();
        d->handScrollMotions = 0;
		viewport()->setCursor(Qt::ClosedHandCursor);

		
	}
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
	if (d->handScrolling) 
	{
	   QPoint delta = event->pos() - d->lastMouseEvent.pos();
		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();
		hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
		vBar->setValue(vBar->value() - delta.y());
		++d->handScrollMotions;
		d->mouseMoveEventHandler(event);
		// update mouse speed (depends from (x,y)' and move length)
	}	
	QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent ( QMouseEvent * event)
{
    viewport()->setCursor(Qt::OpenHandCursor);
    d->handScrolling = false;

    if (scene() && !d->lastMouseEvent.isAccepted() && d->handScrollMotions <= 6) {
        scene()->clearSelection();
    }
    d->storeMouseEvent(event);


	QPoint dragRange = d->mousePressPos - event->pos();
	d->calcMouseSpeed();

	if (d->mouseSpeed > 50.0) 
	{
		d->startAnimationCoordinate = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
		int dur = 200.0 * log(d->mouseSpeed) ;
		d->timeline = new QTimeLine(dur);
		d->timeline->setCurveShape(QTimeLine::EaseOutCurve);
		d->timeline->setUpdateInterval(20); // 60fps
		connect(d->timeline, SIGNAL(valueChanged(qreal)), this, SLOT(onDragMoveAnimation(qreal)));
		//d->timeline->setDuration(1200);
		d->timeline->start();
	}
	/*
    QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseRelease);
    mouseEvent.setWidget(viewport());
    mouseEvent.setButtonDownScenePos(d->mousePressButton, d->mousePressScenePoint);
    mouseEvent.setButtonDownScreenPos(d->mousePressButton, d->mousePressScreenPoint);
    mouseEvent.setScenePos(mapToScene(event->pos()));
    mouseEvent.setScreenPos(event->globalPos());
    mouseEvent.setLastScenePos(d->lastMouseMoveScenePoint);
    mouseEvent.setLastScreenPos(d->lastMouseMoveScreenPoint);
    mouseEvent.setButtons(event->buttons());
    mouseEvent.setButton(event->button());
    mouseEvent.setModifiers(event->modifiers());
    mouseEvent.setAccepted(false);
    QApplication::sendEvent(scene(), &mouseEvent);

    // Update the last mouse event selected state.
    d->lastMouseEvent.setAccepted(mouseEvent.isAccepted());

    if (mouseEvent.isAccepted() && mouseEvent.buttons() == 0 && viewport()->testAttribute(Qt::WA_SetCursor)) {
        // The last mouse release on the viewport will trigger clearing the cursor.
        d->_q_unsetViewportCursor();
    }
	*/
}
/**/
void GraphicsView::onDragMoveAnimation(qreal value) 
{
	qreal range = d->mouseSpeed / 5  * value*5;
	QPoint delta(range * d->moveDx, range * d->moveDy);
	QScrollBar *hBar = horizontalScrollBar();
	QScrollBar *vBar = verticalScrollBar();
	hBar->setValue(d->startAnimationCoordinate.x() + (isRightToLeft() ? delta.x() : -delta.x()));
	vBar->setValue(d->startAnimationCoordinate.y() - delta.y());

	//centerOn(d->startAnimationCoordinate.x() + (isRightToLeft() ? delta.x() : -delta.x()), d->startAnimationCoordinate.y() - delta.y());

	qDebug() << "animation. dx=" << delta.x() << "dy=" << delta.y();
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
