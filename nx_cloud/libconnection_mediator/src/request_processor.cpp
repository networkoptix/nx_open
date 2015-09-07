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
    const auto hostNameAttr = request.getAttribute< stun::cc::attrs::HostName >();
    if( !hostNameAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute HostName is required" );
        return boost::none;
    }

    const auto hostNameParts = hostNameAttr->value.split( '.' );
    if( hostNameParts.size() != 2 )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute HostName should be <ServerId>.<SystemId>" );
        return boost::none;
    }

    const auto authAttr = request.getAttribute< stun::cc::attrs::Authorization >();
    if( !authAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute Authorization is required" );
        return boost::none;
    }

    MediaserverData data = { hostNameParts[1], hostNameParts[0] };

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
