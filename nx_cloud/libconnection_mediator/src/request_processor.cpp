
#include "request_processor.h"

#include <nx/utils/log/log.h>
#include <nx/network/cloud/data/result_code.h>


namespace nx {
namespace hpm {

RequestProcessor::RequestProcessor( AbstractCloudDataProvider* cloudData )
    : m_cloudData( cloudData )
{
}

RequestProcessor::~RequestProcessor()
{
}

api::ResultCode RequestProcessor::getMediaserverData(
    const ConnectionStrongRef& connection,
    stun::Message& request,
    MediaserverData* const foundData,
    nx::String* errorMessage)
{
    const auto systemAttr = request.getAttribute< stun::cc::attrs::SystemId >();
    if (!systemAttr)
    {
        *errorMessage = "Attribute SystemId is required";
        return api::ResultCode::badRequest;
    }

    const auto serverAttr = request.getAttribute< stun::cc::attrs::ServerId >();
    if( !serverAttr )
    {
        *errorMessage = "Attribute ServerId is required";
        return api::ResultCode::badRequest;
    }

    MediaserverData data(
        systemAttr->getString(),
        serverAttr->getString());
    if( !m_cloudData ) // debug mode
    {
        *foundData = data;
        return api::ResultCode::ok;
    }

    const auto system = m_cloudData->getSystem( data.systemId );
    if( !system )
    {
        *errorMessage = "System could not be found";
        return api::ResultCode::notAuthorized;
    }

//    if( !system->mediatorEnabled )    //cloud connect is not 
//    {
//        sendErrorResponse( connection, request.header, stun::error::badRequest,
//                       "Mediator is not enabled for this system" );
//        return boost::none;
//    }

    if (!request.verifyIntegrity(data.systemId, system->authKey))
    {
        NX_LOGX( lm( "Ignoring request (method %1) from %2 with wrong message integrity. Found credentials: %3:%4" )
                 .arg(request.header.method).arg(connection->getSourceAddress().toString()).arg(data.systemId).arg(system->authKey),
                 cl_logDEBUG1 );
        *errorMessage = "Wrong message integrity";
        return api::ResultCode::notAuthorized;
    }

    *foundData = data;
    return api::ResultCode::ok;
}

} // namespace hpm
} // namespace nx
