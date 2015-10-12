#include "request_processor.h"

namespace nx {
namespace hpm {

RequestProcessor::RequestProcessor( CloudDataProviderIf* cloudData )
    : m_cloudData( cloudData )
{
}

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

    MediaserverData data = { systemAttr->value, serverAttr->value };
    const auto system = m_cloudData->getSystem( data.systemId );
    if( !system )
    {
        errorResponse( connection, request.header, stun::cc::error::notFound,
                       "System could not be found" );
        return boost::none;
    }
    if( !system->mediatorEnabled )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Mediator is not enabled for this system" );
        return boost::none;
    }

    const auto authAttr = request.getAttribute< stun::cc::attrs::Authorization >();
    if( !authAttr )
    {
        errorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute Authorization is required" );
        return boost::none;
    }
    if( system->authKey != authAttr->value )
    {
        errorResponse( connection, request.header, stun::error::unauthtorized,
                       "Authorization failed" );
        return boost::none;

    }

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
