#include "slider_item.h"


CLDirectJumpSlider::CLDirectJumpSlider(Qt::Orientation orientation, QWidget * parent ):
QSlider(orientation, parent)
{

}

CLDirectJumpSlider::~CLDirectJumpSlider()
{

}


void CLDirectJumpSlider::mousePressEvent( QMouseEvent * event)
{

	int new_pos = QStyle::sliderValueFromPosition(minimum(),maximum(),event->x(),width(),invertedAppearance());

	int diff = abs(value() - new_pos);

	if (diff > (maximum()-minimum())*0.02 )
	{
		setValue(new_pos);
		emit sliderMoved(new_pos);
	}

	QSlider::mousePressEvent(event);
}