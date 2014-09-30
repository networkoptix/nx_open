/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#include "cloud_connector.h"


namespace nx_cc
{
    bool CloudConnector::setupCloudConnection(
        const HostAddress& hostname,
        std::function<void(nx_cc::ErrorDescription, AbstractStreamSocket*)>&& completionHandler )
    {
        //TODO #ak
        return false;
    }

    CloudConnector* CloudConnector::instance()
    {
        //TODO #ak
        return nullptr;
    }
}
