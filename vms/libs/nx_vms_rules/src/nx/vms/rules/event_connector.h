#pragma once

#include <QObject>

#include "basic_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API EventConnector: public QObject
{
    Q_OBJECT

signals:
    void event(const EventPtr &event);
};

} // namespace nx::vms::rules