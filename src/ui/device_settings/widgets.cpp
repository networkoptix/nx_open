#include "widgets.h"

SettingsSlider::SettingsSlider( Qt::Orientation orientation, QWidget * parent):
QSlider(orientation, parent)
{

}

void SettingsSlider::keyReleaseEvent ( QKeyEvent * event )
{
	emit onKeyReleased();
}
