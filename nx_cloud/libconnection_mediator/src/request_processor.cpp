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
    const auto systemAttr = request.getAttribute< stun::cc::attrs::SystemId >();
    if( !systemAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute SystemId is required" );
        return boost::none;
    }

    const auto serverAttr = request.getAttribute< stun::cc::attrs::ServerId >();
    if( !serverAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute ServerId is required" );
        return boost::none;
    }

    const auto authAttr = request.getAttribute< stun::cc::attrs::Authorization >();
    if( !authAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute Authorization is required" );
        return boost::none;
    }

    MediaserverData data = { systemAttr->value, serverAttr->value };

    // TODO: verify paramiters and authorization in CloudDb
    return data;
}

void RequestProcessor::successResponse(
        const ConnectionSharedPtr& connection, stun::Header& request )
{
    stun::Message response( stun::Header(
        stun::MessageClass::successResponse, request.method,
        std::move( request.transactionId ) ) );

    connection->sendMessage( std::move( response ) );
}

void RequestProcessor::errorResponse(
        const ConnectionSharedPtr& connection, stun::Header& request,
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
