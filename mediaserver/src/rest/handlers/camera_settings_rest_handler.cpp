/**********************************************************
* 01 aug 2012
* a.kolesnikov
***********************************************************/

#include "camera_settings_rest_handler.h"

#include <algorithm>

#include <QtCore/QElapsedTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/param.h>


//!max time (milliseconds) to wait for async operation completion
static const unsigned long MAX_WAIT_TIMEOUT_MS = 15000;

namespace HttpStatusCode
{
    // TODO: #Elric #enum
    enum Value
    {
        ok = 200,
        badRequest = 400,
        notFound = 404
    };
}

int QnCameraSettingsRestHandler::executeGet( const QString& path, const QnRequestParamList& params, QByteArray& responseMessageBody, QByteArray& contentType)
{
    Q_UNUSED(contentType)
        // TODO: #Elric #enum
    enum CmdType
    {
        ctSetParam,
        ctGetParam
    } cmdType;
    //LTRACE( 6, http, "QnCameraSettingsHandler. Received request "<<path );

    //TODO it would be nice to fill reasonPhrase too
    if( params.empty() )
        return HttpStatusCode::badRequest;

    QnRequestParamList::const_iterator camIDIter = params.begin();
    for( ;
        camIDIter != params.end();
        ++camIDIter )
    {
        if( camIDIter->first == "res_id" )
            break;
    }
    if( camIDIter == params.end() )
    {
        //LTRACE( 4, http, "QnCameraSettingsHandler. No res_id params in request "<<path<<". Ignoring..." );
        return HttpStatusCode::badRequest;
    }
    const QString& camID = camIDIter->second;

    if( params.size() == 1 )
    {
        //LTRACE( 4, http, "QnCameraSettingsHandler. No param names in request "<<path );
        return HttpStatusCode::badRequest;
    }

    QStringList pathItems = path.split( '/' );
    if( pathItems.empty() )
        return HttpStatusCode::notFound;
    if( pathItems.back().isEmpty() )
    {
        pathItems.pop_back();
        if( pathItems.empty() )
            return HttpStatusCode::notFound;
    }
    const QString& cmdName = pathItems.back();
    if( cmdName == "getCameraParam" )
    {
        cmdType = ctGetParam;
    }
    else if( cmdName == "setCameraParam" )
    {
        cmdType = ctSetParam;
    }
    else
    {
        //LTRACE( 4, http, "QnCameraSettingsHandler. Unknown command "<<cmdName<<" in request "<<path<<". Ignoring..." );
        return HttpStatusCode::notFound;
    }

    AwaitedParameters awaitedParams;
    const QnResourcePtr& res = QnResourcePool::instance()->getNetResourceByPhysicalId( camID );
    if( !res.data() )
    {
        //LTRACE( 4, http, "QnCameraSettingsHandler. Unknown camera "<<camID<<" in request "<<path<<". Ignoring..." );
        return HttpStatusCode::notFound;
    }
    awaitedParams.resource = res;

    if( cmdType == ctGetParam )
        connect(
            res.data(),
            SIGNAL(asyncParamGetDone(const QnResourcePtr &, const QString&, const QVariant&, bool)),
            this,
            SLOT(asyncParamGetComplete(const QnResourcePtr &, const QString&, const QVariant&, bool)),
            Qt::DirectConnection );
    else
        connect(
            res.data(),
            SIGNAL(asyncParamSetDone(const QnResourcePtr &, const QString&, const QVariant&, bool)),
            this,
            SLOT(asyncParamSetComplete(const QnResourcePtr &, const QString&, const QVariant&, bool)),
            Qt::DirectConnection );
    for( QnRequestParamList::const_iterator
        it = params.begin();
        it != params.end();
        ++it )
    {
        if( it == camIDIter )
            continue;
        if( cmdType == ctSetParam )
            res->setParamAsync( it->first, it->second, QnDomainPhysical );
        else if( cmdType == ctGetParam )
            res->getParamAsync( it->first, QnDomainPhysical );
        awaitedParams.paramsToWaitFor.insert( it->first );
    }

    {
        QMutexLocker lk( &m_mutex );
        std::set<AwaitedParameters*>::iterator awaitedParamsIter = m_awaitedParamsSets.insert( &awaitedParams ).first;
        QElapsedTimer asyncOpCompletionTimer;
        asyncOpCompletionTimer.start();
        for( ;; )
        {
            const qint64 curClock = asyncOpCompletionTimer.elapsed();
            if( curClock >= (int)MAX_WAIT_TIMEOUT_MS )
            {
                //out of time, returning what we have...
                for( std::set<QString>::iterator
                    it = awaitedParams.paramsToWaitFor.begin();
                    it != awaitedParams.paramsToWaitFor.end();
                     )
                {
                    awaitedParams.paramValues.insert( std::make_pair( *it, std::make_pair( QVariant(), false ) ) ); //this param async operation has not completed in time
                    awaitedParams.paramsToWaitFor.erase( it++ );
                }
                break;
            }
            m_cond.wait( &m_mutex, MAX_WAIT_TIMEOUT_MS - curClock );
            if( awaitedParams.paramsToWaitFor.empty() )
                break;
        }
        m_awaitedParamsSets.erase( awaitedParamsIter );
    }

    if( cmdType == ctGetParam )
        disconnect(
            res.data(),
            SIGNAL(asyncParamGetDone(const QnResourcePtr &, const QString&, const QVariant&, bool)),
            this,
            SLOT(asyncParamGetComplete(const QnResourcePtr &, const QString&, const QVariant&, bool)) );
    else
        disconnect(
            res.data(),
            SIGNAL(asyncParamSetDone(const QnResourcePtr &, const QString&, const QVariant&, bool)),
            this,
            SLOT(asyncParamSetComplete(const QnResourcePtr &, const QString&, const QVariant&, bool)) );

    // TODO: #Elric use JSON here
    {
        //serializing answer
        for( std::map<QString, std::pair<QVariant, bool> >::const_iterator
            it = awaitedParams.paramValues.begin();
            it != awaitedParams.paramValues.end();
            ++it )
        {
            if( cmdType == ctGetParam )
                responseMessageBody += it->first + "=" + it->second.first.toString() + "\n";
            else if( cmdType == ctSetParam )
                responseMessageBody += (it->second.second ? "ok:" : "failure:") + it->first + "\n";
        }
    }

    //LTRACE( 6, http, "QnCameraSettingsHandler. request "<<path<<" processed successfully" );

    return HttpStatusCode::ok;
}

int QnCameraSettingsRestHandler::executePost( const QString& /*path*/, const QnRequestParamList& /*params*/, const QByteArray& /*body*/, QByteArray& /*responseMessageBody*/, QByteArray& /*contentType*/)
{
    //TODO/IMPL
    return 0;
}

//QString QnCameraSettingsHandler::description( TCPSocket* /*tcpSocket*/ ) const
//{
//  return 
//      "Allows to get/set camera parameters<BR>\n"
//      "To get: <BR>\n"
//      "c->s: GET /api/getCameraParam?res_id=camera_mac&param1&param2&param3 HTTP/1.0 <BR>\n"
//      "s->c: HTTP/1.0 200 OK<BR>\n"
//      "      Content-Type: application/text<BR>\n"
//      "      Content-Length: 39<BR>\n"
//      "      <BR>\n"
//      "      param1=value1<BR>\n"
//      "      param2=value2<BR>\n"
//      "      param3=value3<BR>\n"
//      "<BR>\n"
//      "To set: <BR>\n"
//      "c->s: GET /api/setCameraParam?res_id=camera_mac&param1=value1&param2=value2&param3=value3 HTTP/1.0 <BR>\n"
//      "s->c: HTTP/1.0 500 Internal Server Error<BR>\n"
//      "      Content-Type: application/text<BR>\n"
//      "      Content-Length: 38<BR>\n"
//        "      <BR>\n"
//      "      ok: param1<BR>\n"
//      "      failure: param2<BR>\n"
//      "      ok: param3<BR>\n";
//
//}

void QnCameraSettingsRestHandler::asyncParamGetComplete(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result )
{
    QMutexLocker lk( &m_mutex );

    //LTRACE( 6, http, "QnCameraSettingsHandler::asyncParamGetComplete. paramName "<<paramName<<", paramValue "<<paramValue );

    for( std::set<AwaitedParameters*>::const_iterator
        it = m_awaitedParamsSets.begin();
        it != m_awaitedParamsSets.end();
        ++it )
    {
        if((*it)->resource != resource)
            continue;

        std::set<QString>::iterator paramIter = (*it)->paramsToWaitFor.find( paramName );
        if( paramIter == (*it)->paramsToWaitFor.end() )
            continue;
        //context *it is waiting for this parameter, giving it to him...
        (*it)->paramValues.insert( std::make_pair( paramName, std::make_pair( paramValue, result ) ) );
        (*it)->paramsToWaitFor.erase( paramIter );
    }

    m_cond.wakeAll();
}

void QnCameraSettingsRestHandler::asyncParamSetComplete(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result )
{
    //processing is identical to the previous method
    asyncParamGetComplete(resource, paramName, paramValue, result);
}

QString QnSetCameraParamRestHandler::description() const
{
    return
        "Sets values of several camera parameters.<BR>"
        "Request format: GET /api/setCameraParam?res_id=camera_mac&amp;paramName1=paramValue1&amp;paramName2=paramValue2&amp;...<BR>"
        "Returns OK if all parameters have been set, otherwise returns error 500 (Internal server error) and result of setting every param<BR>";
}

QString QnGetCameraParamRestHandler::description() const
{
    return
        "Returns list of camera parameters.<BR>"
        "Request format: GET /api/getCameraParam?res_id=camera_mac&amp;paramName1&amp;paramName2&amp;...<BR>"
        "Returns required parameter values in form of paramName=paramValue, each param on new line<BR>";
}
