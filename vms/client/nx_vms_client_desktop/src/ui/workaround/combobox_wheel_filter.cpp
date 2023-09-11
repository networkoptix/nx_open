// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "combobox_wheel_filter.h"

#include <QComboBox>
#include <QEvent>

bool ComboboxWheelFilter::eventFilter(QObject* watched, QEvent* event)
{
    return event->type() == QEvent::Wheel && qobject_cast<QComboBox*>(watched);
}
