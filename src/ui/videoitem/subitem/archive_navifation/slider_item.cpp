#include "slider_item.h"

#include <QStyle>
#include <QMouseEvent>

CLDirectJumpSlider::CLDirectJumpSlider(Qt::Orientation orientation, QWidget * parent ):
QSlider(orientation, parent)
{

}

CLDirectJumpSlider::~CLDirectJumpSlider()
{

}


void CLDirectJumpSlider::mousePressEvent( QMouseEvent * event)
{
	setValue(QStyle::sliderValueFromPosition(minimum(),maximum(),event->x(),width(),invertedAppearance()));
	QSlider::mousePressEvent(event);
}