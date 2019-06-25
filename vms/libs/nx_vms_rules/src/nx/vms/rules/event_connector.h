#pragma once

#include <QObject>

#include "basic_event.h"

namespace nx::vms::rules {

using EventPtr = QSharedPointer<BasicEvent>;

class NX_VMS_RULES_API EventConnector: public QObject
{
    Q_OBJECT

signals:
    void vmsEvent(const EventPtr &event);
};

} // namespace nx::vms::rules