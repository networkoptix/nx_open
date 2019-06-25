#include "engine.h"

namespace nx::vms::rules {

Engine::Engine()
{
}

bool Engine::addEventConnector(EventConnector *eventConnector)
{
    if (m_connectors.contains(eventConnector))
        return false;

    connect(eventConnector, &EventConnector::vmsEvent, this, &Engine::processVmsEvent);
    m_connectors.push_back(eventConnector);
    return true;
}

void Engine::processVmsEvent(const EventPtr &event)
{
    qDebug() << "Processing " << event->type();
    qDebug() << event->property("action").toString();
}

} // namespace nx::vms::rules
