#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/stun/message_dispatcher.h>

#include "cloud_data_provider.h"
#include "server/stun_request_processing_helper.h"


namespace nx {
namespace hpm {

struct MediaserverData
{
    String systemId;
    String serverId;

    MediaserverData() {}
    MediaserverData(
        String _systemId,
        String _serverId)
    :
        systemId(std::move(_systemId)),
        serverId(std::move(_serverId))
    {
    }

    /** Domain labels are compared starting with top-level domain. */
    bool operator<(const MediaserverData& rhs) const
    {
        if (systemId < rhs.systemId)
            return true;
        else if (rhs.systemId < systemId)
            return false;
        return serverId < rhs.serverId;
    }

    String hostName() const
    {
        return systemId + "." + serverId;
    }
};

/** Abstract request processor (helper base class)
 *
 *  \todo: Attributes parsing for common functionality
 *  \todo: Add authorization
 */
class RequestProcessor
{
public:
    RequestProcessor( AbstractCloudDataProvider* cloudData );
    virtual ~RequestProcessor() = 0;

protected:

    /** Returns mediaserver data from \a request,
     *  sends \fn errorResponse in case of failure */
    boost::optional< MediaserverData > getMediaserverData(
            ConnectionStrongRef connection, stun::Message& request );

private:
    AbstractCloudDataProvider* m_cloudData;
};

} // namespace hpm
} // namespace nx

#endif // REQUEST_PROCESSOR_H
