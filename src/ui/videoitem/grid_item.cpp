#include "grid_item.h"
#include "ui/video_cam_layout/grid_engine.h"
#include "ui/graphicsview.h"
#include "ui/animation/property_animation.h"


static const int base_line_width = 40;

CLGridItem::CLGridItem(GraphicsView* view):
m_view(view),
m_alpha(0),
m_animation(0)
{
	setZValue(-1);
	setVisible(false);
}

CLGridItem::~CLGridItem()
{
	stopAnimation();
}

void CLGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

	CLGridEngine& ge = m_view->getCamLayOut().getGridEngine();

	int unit_width = ge.getSettings().totalSlotWidth();
	int unit_height = ge.getSettings().totalSlotHeight();
	QRect grid_rect = ge.gridSlotRect();

	int total_width = unit_width*grid_rect.width();
	int total_height = unit_height*grid_rect.height();

	int line_width = base_line_width + qMin( qMax(total_width, total_height)*4 ,100);

	int left = grid_rect.left()*unit_width;
	int top = grid_rect.top()*unit_height;

	QColor color(0,240,240,m_alpha);

	painter->setPen(QPen(color, base_line_width, Qt::SolidLine));

	for(int i = grid_rect.left(); i <= grid_rect.right() + 1; ++i) // vertical lines
	{
		QPointF p1(i*unit_width, top);
		QPointF p2(i*unit_width, top + total_height);
		painter->drawLine(p1,p2);
	}

	for(int i = grid_rect.top(); i <= grid_rect.bottom() + 1; ++i)// horizontal lines
	{
		QPointF p1( left, i*unit_height);
		QPointF p2( left + total_width, i*unit_height);
		painter->drawLine(p1,p2);
	}

}

QRectF CLGridItem::boundingRect() const
{
	//1) we are not going to interect with this iyem 
	//2) we use unbounded drawing 

	// almost(!!) never mind what this function returns;

	/*
	CLGridEngine& ge = m_view->getCamLayOut().getGridEngine();
	int unit_width = ge.getSettings().totalSlotWidth();
	int unit_height = ge.getSettings().totalSlotHeight();
	QRect grid_rect = ge.gridSlotRect();
	return QRectF(0, 0, unit_width*grid_rect.width(), unit_height*grid_rect.height());
	/**/

	return QRectF(0, 0, 100000,100000);
}

void CLGridItem::show(int time_ms)
{
	stopAnimation();
	setVisible(true);

	m_animation = AnimationManager::instance().addAnimation(this, "alpha");
	m_animation->setDuration(time_ms);
	m_animation->setEasingCurve(QEasingCurve::InOutSine);
	m_animation->setStartValue(alpha());
	m_animation->setEndValue(255);
	m_animation->start();	

	connect(m_animation, SIGNAL(finished()), this, SLOT(stopAnimation()));

}

void CLGridItem::hide(int time_ms )
{
	stopAnimation();

	m_animation = AnimationManager::instance().addAnimation(this, "alpha");
	m_animation->setDuration(time_ms);
	m_animation->setEasingCurve(QEasingCurve::InOutSine);
	m_animation->setStartValue(alpha());
	m_animation->setEndValue(0);
	m_animation->start();	

	connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));

}

void CLGridItem::stopAnimation()
{
	if (m_animation)
	{
		m_animation->stop();
		delete m_animation;
		m_animation = 0;
	}

	if (m_alpha==0)
		setVisible(false);
}

int CLGridItem::alpha() const
{
	return m_alpha;
}

void CLGridItem::setAlpha(int val)
{
	m_alpha = val;
	update();
}
