#include "request_processor.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace hpm {

RequestProcessor::RequestProcessor( AbstractCloudDataProvider* cloudData )
    : m_cloudData( cloudData )
{
}

RequestProcessor::~RequestProcessor()
{
}

boost::optional< RequestProcessor::MediaserverData >
    RequestProcessor::getMediaserverData(
        ConnectionStrongRef connection, stun::Message& request)
{
    const auto systemAttr = request.getAttribute< stun::cc::attrs::SystemId >();
    if( !systemAttr )
    {
        sendErrorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute SystemId is required" );
        return boost::none;
    }

    const auto serverAttr = request.getAttribute< stun::cc::attrs::ServerId >();
    if( !serverAttr )
    {
        sendErrorResponse( connection, request.header, stun::error::badRequest,
                       "Attribute ServerId is required" );
        return boost::none;
    }

    MediaserverData data = { systemAttr->getString(), serverAttr->getString() };
    if( !m_cloudData ) // debug mode
        return data;

    const auto system = m_cloudData->getSystem( data.systemId );
    if( !system )
    {
        sendErrorResponse( connection, request.header, stun::cc::error::notFound,
                       "System could not be found" );
        return boost::none;
    }
//    if( !system->mediatorEnabled )
//    {
//        sendErrorResponse( connection, request.header, stun::error::badRequest,
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

} // namespace hpm
} // namespace nx
