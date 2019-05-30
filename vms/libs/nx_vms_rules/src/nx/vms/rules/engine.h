#pragma once

#include <QList>
#include <QObject>

#include <nx/utils/singleton.h>

#include "event_connector.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API Engine:
    public QObject,
    public Singleton<Engine>
{
    Q_OBJECT

public:
    Engine();

    bool addEventConnector(EventConnector *eventConnector);

private:
    void processVmsEvent(const EventPtr &event);

private:
    QList<EventConnector *> m_connectors;
};

} // namespace nx::vms::rules