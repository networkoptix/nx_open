/**********************************************************
* 01 aug 2012
* a.kolesnikov
***********************************************************/

#include "camera_settings_rest_handler.h"

#include <algorithm>

#include <QtCore/QElapsedTimer>

#include <utils/common/log.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
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

struct AwaitedParameters
{
    QnResourcePtr resource;

    //!Parameters which values we are waiting for
    std::set<QString> paramsToWaitFor;
    //!New parameter values are stored here
    std::map<QString, std::pair<QVariant, bool> > paramValues;
};

int QnCameraSettingsRestHandler::executeGet( const QString& path, const QnRequestParamList& params, QByteArray& responseMessageBody, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(contentType)
        // TODO: #Elric #enum
    enum CmdType
    {
        ctSetParam,
        ctGetParam
    } cmdType;
    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Received request %1").arg(path), cl_logDEBUG1 );

    QnRequestParamList locParams = params;

    QnRequestParamList::iterator serverGuidIter = locParams.find(lit("x-server-guid"));
    if (serverGuidIter != locParams.end())
        locParams.erase(serverGuidIter);

    //TODO it would be nice to fill reasonPhrase too
    if( locParams.empty() )
        return HttpStatusCode::badRequest;

    QnRequestParamList::const_iterator camIDIter = locParams.find(lit("res_id"));
    if( camIDIter == locParams.end() )
    {
        NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. No res_id param in request %1. Ignoring...").arg(path), cl_logWARNING );
        return HttpStatusCode::badRequest;
    }
    const QString& camID = camIDIter->second;

    if( params.size() == 1 )
    {
        NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. No param names in request %1").arg(path), cl_logWARNING );
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
        NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Unknown command %1 in request %2. Ignoring...").arg(cmdName).arg(path), cl_logWARNING );
        return HttpStatusCode::notFound;
    }

    AwaitedParameters awaitedParams;
    const QnResourcePtr& res = QnResourcePool::instance()->getNetResourceByPhysicalId( camID );
    if( !res.data() )
    {
        NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Unknown camera %1 in request %2. Ignoring...").arg(camID).arg(path), cl_logWARNING );
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
        it = locParams.begin();
        it != locParams.end();
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
    contentType = "text/plain";

    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. request %1 processed successfully").arg(path), cl_logDEBUG1 );

    return HttpStatusCode::ok;
}

int QnCameraSettingsRestHandler::executePost(const QString& /*path*/, const QnRequestParamList& /*params*/, 
    const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& /*responseMessageBody*/, QByteArray& /*contentType*/, const QnRestConnectionProcessor*)
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

    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler::asyncParamGetComplete. paramName %1, paramValue %2").arg(paramName).arg(paramValue.toString()), cl_logDEBUG1 );

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

