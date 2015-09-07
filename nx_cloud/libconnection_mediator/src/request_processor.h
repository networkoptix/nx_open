#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include <utils/network/stun/server_connection.h>
#include <utils/network/stun/message_dispatcher.h>
#include <utils/network/stun/cc/custom_stun.h>

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
};

} // namespace hpm
} // namespace nx

#endif // REQUEST_PROCESSOR_H
