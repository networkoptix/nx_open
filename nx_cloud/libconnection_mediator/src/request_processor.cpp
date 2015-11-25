#include "request_processor.h"

#include <nx/tool/log/log.h>

namespace nx {
namespace hpm {

RequestProcessor::RequestProcessor( CloudDataProviderBase* cloudData )
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

    MediaserverData data = { systemAttr->getString(), serverAttr->getString() };
    if( !m_cloudData ) // debug mode
        return data;

    const auto system = m_cloudData->getSystem( data.systemId );
    if( !system )
    {
        errorResponse( connection, request.header, stun::cc::error::notFound,
                       "System could not be found" );
        return boost::none;
    }
//    if( !system->mediatorEnabled )
//    {
//        errorResponse( connection, request.header, stun::error::badRequest,
//                       "Mediator is not enabled for this system" );
//        return boost::none;
//    }

    if( request.verifyIntegrity( data.systemId, system->authKey ) )
    {
        NX_LOGX( lm( "Ignore request from %1 with wrong message integrity" )
                 .arg( data.systemId ), cl_logWARNING );
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
