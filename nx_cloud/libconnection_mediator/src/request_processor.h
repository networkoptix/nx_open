#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include <utils/network/stun/server_connection.h>
#include <utils/network/stun/message_dispatcher.h>

#include "stun/custom_stun.h"

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
    // TODO: provide constructor savid CloudDb connection when we have such

    virtual ~RequestProcessor() = 0;

    typedef std::shared_ptr< stun::ServerConnection > ConnectionSharedPtr;
    typedef std::shared_ptr< stun::ServerConnection > ConnectionWeakPtr;

protected:
    struct MediaserverData { QnUuid systemId, serverId; };

    /** Returns mediaserver data from \a request,
     *  sends \fn errorResponse in case of failure */
    boost::optional< MediaserverData > getMediaserverData(
            ConnectionSharedPtr connection, stun::Message& request );

    /** Send error responce with error code and description */
    static void errorResponse(
            ConnectionSharedPtr connection, stun::Header& request,
            int code, String reason );
};

} // namespace hpm
} // namespace nx

#endif // REQUEST_PROCESSOR_H
