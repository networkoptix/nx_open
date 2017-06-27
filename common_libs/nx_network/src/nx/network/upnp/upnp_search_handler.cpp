#include "upnp_search_handler.h"

#include "upnp_device_searcher.h"

namespace nx_upnp {

SearchAutoHandler::SearchAutoHandler(const QString& devType)
{
    DeviceSearcher::instance()->registerHandler(this, devType);
}

SearchAutoHandler::~SearchAutoHandler()
{
    if (auto searcher = DeviceSearcher::instance())
        searcher->unregisterHandler(this);
}

} // namespace nx_upnp
