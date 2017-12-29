#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/message_dispatcher.h>

#include "cloud_data_provider.h"
#include "server/stun_request_processing_helper.h"


namespace nx {
namespace hpm {

/** TODO #ak replace this structure with something not related to "system" and "server" */
struct MediaserverData
{
    String systemId;
    String serverId;

    MediaserverData(
        String _systemId = String(),
        String _serverId = String())
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
        return serverId + "." + systemId;
    }

    QString toString() const
    {
        return QString::fromUtf8(hostName());
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

    /** Returns mediaserver data from \a request */
    api::ResultCode getMediaserverData(
        const nx::network::stun::AbstractServerConnection& connection,
        network::stun::Message& request,
        MediaserverData* const foundData,
        nx::String* errorMessage);

private:
    AbstractCloudDataProvider* m_cloudData;
};

} // namespace hpm
} // namespace nx

#endif // REQUEST_PROCESSOR_H
