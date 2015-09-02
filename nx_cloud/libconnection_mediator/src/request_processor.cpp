#include "request_processor.h"

namespace nx {
namespace hpm {

RequestProcessor::~RequestProcessor()
{
}

boost::optional< RequestProcessor::MediaserverData >
    RequestProcessor::getMediaserverData(
        ConnectionSharedPtr connection, stun::Message& request)
{
    // TODO: verify paramiters and authorization in CloudDb

    const auto systemAttr = request.getAttribute< attrs::SystemId >();
    if( !systemAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute SystemId is required" );
        return boost::none;
    }

    auto systemId = systemAttr->get();
    if( systemId.isNull() )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute SystemId is not a valid UUID" );
        return boost::none;
    }

    const auto serverAttr = request.getAttribute< attrs::ServerId >();
    if( !serverAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute ServerId is required" );
        return boost::none;
    }

    auto serverId = serverAttr->get();
    if( serverId.isNull() )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute ServerId is not a valid UUID" );
        return boost::none;
    }

    const auto authAttr = request.getAttribute< attrs::Authorization >();
    if( !authAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute Authorization is required" );
        return boost::none;
    }

    MediaserverData data = { std::move( systemId ), std::move( serverId ) };
    return data;
}

void RequestProcessor::errorResponse(
        ConnectionSharedPtr connection, stun::Header& request,
        int code, String reason )
{
    stun::Message response( stun::Header(
        stun::MessageClass::errorResponse, request.method,
        std::move( request.transactionId ) ) );

    response.newAttribute< stun::attrs::ErrorDescription >( code, std::move(reason) );
    connection->sendMessage( std::move( response ) );
}

} // namespace hpm
} // namespace nx
