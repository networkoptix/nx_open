/**********************************************************
* 01 aug 2012
* a.kolesnikov
***********************************************************/

#include "camera_settings_rest_handler.h"

#include <algorithm>

#include <QtCore/QElapsedTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/param.h>
#include <core/resource/camera_advanced_param.h>

#include <utils/common/log.h>
#include <utils/common/model_functions.h>
#include <utils/network/tcp_connection_priv.h>


namespace {

	//!max time (milliseconds) to wait for async operation completion
	static const unsigned long MAX_WAIT_TIMEOUT_MS = 15000;

	QSet<QString> allParameterIds(const QnCameraAdvancedParamGroup& group) {
		QSet<QString> result;
		for (const QnCameraAdvancedParamGroup &subGroup: group.groups)
			result.unite(allParameterIds(subGroup));
		for (const QnCameraAdvancedParameter &param: group.params)
			if (!param.getId().isEmpty())
				result.insert(param.getId());
		return result;
	}

	QSet<QString> allParameterIds(const QnCameraAdvancedParams& params) {
		QSet<QString> result;
		for (const QnCameraAdvancedParamGroup &group: params.groups)
			result.unite(allParameterIds(group));
		return result;
	}
}



struct AwaitedParameters {
    QnResourcePtr resource;
	QSet<QString> requested;
    QnCameraAdvancedParamValueList result;
};

QnCameraSettingsRestHandler::QnCameraSettingsRestHandler():
	base_type(),
	m_paramsReader(new QnCameraAdvancedParamsReader())
{
}

QnCameraSettingsRestHandler::~QnCameraSettingsRestHandler()
{}

int QnCameraSettingsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) {

    enum CmdType
    {
        ctSetParam,
        ctGetParam
    } cmdType;
    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Received request %1").arg(path), cl_logDEBUG1 );

	if (!params.contains(lit("res_id"))) {
		NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. No res_id param in request %1. Ignoring...").arg(path), cl_logWARNING );
		return CODE_BAD_REQUEST;
	}

	QString cameraPhysicalId = params[lit("res_id")];
	QnResourcePtr camera = QnResourcePool::instance()->getNetResourceByPhysicalId( cameraPhysicalId );
	if(!camera) {
		NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Unknown camera %1 in request %2. Ignoring...").arg(cameraPhysicalId).arg(path), cl_logWARNING );
		return CODE_NOT_FOUND;
	}

	/* Clean params that are not keys. */
    QnRequestParams locParams = params;
	locParams.remove(lit("x-server-guid"));
	locParams.remove(lit("res_id"));

	/* Filter allower parameters. */
	auto allowedParams = m_paramsReader->params(camera);
	QSet<QString> allowedIds = allParameterIds(allowedParams);

	for(auto iter = locParams.begin(); iter != locParams.end();) {
		if (allowedIds.contains(iter.key()))
			++iter;
		else
			iter = locParams.erase(iter);
	}

    //TODO it would be nice to fill reasonPhrase too
    if( locParams.empty() ) {
		NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. No valid param names in request %1").arg(path), cl_logWARNING );
        return CODE_BAD_REQUEST;
	}

	QString action = extractAction(path);
    if( action == "getCameraParam" )
    {
        cmdType = ctGetParam;
    }
    else if( action == "setCameraParam" )
    {
        cmdType = ctSetParam;
    }
    else
    {
        NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Unknown command %1 in request %2. Ignoring...").arg(action).arg(path), cl_logWARNING );
        return CODE_NOT_FOUND;
    }

    AwaitedParameters awaitedParams;

    awaitedParams.resource = camera;

	if( cmdType == ctGetParam )
		connect(camera.data(), &QnResource::asyncParamGetDone, this, &QnCameraSettingsRestHandler::asyncParamGetComplete, Qt::DirectConnection);
	else
		connect(camera.data(), &QnResource::asyncParamSetDone, this, &QnCameraSettingsRestHandler::asyncParamSetComplete, Qt::DirectConnection);

    for(auto iter = locParams.cbegin(); iter != locParams.cend(); ++iter) {
		QnCameraAdvancedParamValue param(iter.key(), iter.value());
		awaitedParams.requested << param.id;
        if( cmdType == ctSetParam ) {
			qDebug() << "set parameter" << param.id << "to" << param.value;
            camera->setParamPhysicalAsync(param.id, param.value);
		}
        else if( cmdType == ctGetParam ) {
			qDebug() << "requested parameter" << param.id;
            camera->getParamPhysicalAsync(param.id);
		}
    }

    {
        QMutexLocker lk( &m_mutex );
        std::set<AwaitedParameters*>::iterator awaitedParamsIter = m_awaitedParamsSets.insert( &awaitedParams ).first;
        QElapsedTimer asyncOpCompletionTimer;
        asyncOpCompletionTimer.start();
        for( ;; )
        {
            const qint64 curClock = asyncOpCompletionTimer.elapsed();

			/* Out of time, returning what we have. */
            if( curClock >= (int)MAX_WAIT_TIMEOUT_MS )
                break;
            
            m_cond.wait( &m_mutex, MAX_WAIT_TIMEOUT_MS - curClock );

			/* Received all parameters. */
            if( awaitedParams.requested.empty() )
                break;
        }
        m_awaitedParamsSets.erase( awaitedParamsIter );
    }

    if( cmdType == ctGetParam )
        disconnect(camera.data(), &QnResource::asyncParamGetDone, this, &QnCameraSettingsRestHandler::asyncParamGetComplete);
    else
        disconnect(camera.data(), &QnResource::asyncParamSetDone, this, &QnCameraSettingsRestHandler::asyncParamSetComplete);

    //serializing answer
	QnCameraAdvancedParamValueList reply = awaitedParams.result;
	result.setReply(reply);

    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. request %1 processed successfully").arg(path), cl_logDEBUG1 );
    return CODE_OK;
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

	qDebug() << "param processing complete" << paramName << paramValue.toString();
    for( std::set<AwaitedParameters*>::const_iterator
        it = m_awaitedParamsSets.begin();
        it != m_awaitedParamsSets.end();
        ++it )
    {
		auto awaitedParams = (*it);
        if(!awaitedParams || awaitedParams->resource != resource)
            continue;

		if (!awaitedParams->requested.contains(paramName))
			continue;

		if (result)
			awaitedParams->result << QnCameraAdvancedParamValue(paramName, paramValue.toString());
		awaitedParams->requested.remove(paramName);
    }

    m_cond.wakeAll();
}

void QnCameraSettingsRestHandler::asyncParamSetComplete(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result )
{
    //processing is identical to the previous method
    asyncParamGetComplete(resource, paramName, paramValue, result);
}


