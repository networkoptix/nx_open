#include "combobox_wheel_filter.h"

#include <QComboBox>
#include <QEvent>

bool ComboboxWheelFilter::eventFilter(QObject* watched, QEvent* event)
{
    return event->type() == QEvent::Wheel && qobject_cast<QComboBox*>(watched);
}