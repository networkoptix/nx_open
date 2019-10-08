#include "upnp_search_handler.h"
#include "upnp_device_searcher.h"

namespace nx::network::upnp {

SearchAutoHandler::SearchAutoHandler(
    nx::network::upnp::DeviceSearcher* deviceSearcher,
    const QString& devType)
    :
    m_deviceSearcher(deviceSearcher)
{
    m_deviceSearcher->registerHandler(this, devType);
}

SearchAutoHandler::~SearchAutoHandler()
{
    m_deviceSearcher->unregisterHandler(this);
}

nx::network::upnp::DeviceSearcher* SearchAutoHandler::deviceSearcher() const
{
    return m_deviceSearcher;
}

} // namespace nx::network::upnp
