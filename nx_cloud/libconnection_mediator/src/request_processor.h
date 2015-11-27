#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/cc/custom_stun.h>

#include "cloud_data_provider.h"

namespace nx {
namespace hpm {

/** Abstract request processor (halper base class)
 *
 *  \todo: Attributes parsing for common functionality
 *  \todo: Add authorization
 */
class RequestProcessor
{
public:
    RequestProcessor( CloudDataProviderBase* cloudData );
    virtual ~RequestProcessor() = 0;

    typedef std::shared_ptr< stun::ServerConnection > ConnectionSharedPtr;
    typedef std::weak_ptr< stun::ServerConnection > ConnectionWeakPtr;

protected:
    struct MediaserverData { String systemId, serverId; };

    /** Returns mediaserver data from \a request,
     *  sends \fn errorResponse in case of failure */
    boost::optional< MediaserverData > getMediaserverData(
            ConnectionSharedPtr connection, stun::Message& request );

    /** Send success responce without attributes */
    static void successResponse(
            const ConnectionSharedPtr& connection, stun::Header& request );

    /** Send error responce with error code and description as attribute */
    static void errorResponse(
            const ConnectionSharedPtr& connection, stun::Header& request,
            int code, String reason );

private:
    CloudDataProviderBase* m_cloudData;
};

} // namespace hpm
} // namespace nx

#endif // REQUEST_PROCESSOR_H
