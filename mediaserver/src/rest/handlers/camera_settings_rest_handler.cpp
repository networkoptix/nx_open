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

#include <utils/xml/camera_advanced_param_reader.h>


namespace {
	//!max time (milliseconds) to wait for async operation completion
	static const unsigned long MAX_WAIT_TIMEOUT_MS = 15000;
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

	/* Filter allowed parameters. */
    QnCameraAdvancedParams cameraParameters = m_paramsReader->params(camera);
	auto allowedParams = cameraParameters.allParameterIds();

	for(auto iter = locParams.begin(); iter != locParams.end();) {
		if (allowedParams.contains(iter.key()))
			++iter;
		else
			iter = locParams.erase(iter);
	}

    //TODO it would be nice to fill reasonPhrase too
    if( locParams.empty() ) {
		NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. No valid param names in request %1").arg(path), cl_logWARNING );
        return CODE_BAD_REQUEST;
	}

    Operation operation;
	QString action = extractAction(path);
    if( action == "getCameraParam" ) {
        if (cameraParameters.packet_mode)
            operation = Operation::GetParamsBatch;
        else
            operation = Operation::GetParam;
    }
    else if( action == "setCameraParam" ) {
        if (cameraParameters.packet_mode)
            operation = Operation::SetParamsBatch;
        else
            operation = Operation::SetParam;
    }
    else
    {
        NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. Unknown command %1 in request %2. Ignoring...").arg(action).arg(path), cl_logWARNING );
        return CODE_NOT_FOUND;
    }

    AwaitedParameters awaitedParams;

    awaitedParams.resource = camera;

    connectToResource(camera, operation);

    QnCameraAdvancedParamValueList values;
    for(auto iter = locParams.cbegin(); iter != locParams.cend(); ++iter) {
		QnCameraAdvancedParamValue param(iter.key(), iter.value());
        values << param;
		awaitedParams.requested << param.id;
    }
    processOperation(camera, operation, values);

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

    disconnectFromResource(camera, operation);

    //serializing answer
	QnCameraAdvancedParamValueList reply = awaitedParams.result;
	result.setReply(reply);

    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler. request %1 processed successfully").arg(path), cl_logDEBUG1 );
    return CODE_OK;
}

void QnCameraSettingsRestHandler::connectToResource(const QnResourcePtr &resource, Operation operation) {
    switch (operation) {
    case Operation::GetParam:
        connect(resource, &QnResource::asyncParamGetDone, this, &QnCameraSettingsRestHandler::asyncParamGetComplete, Qt::DirectConnection);
        break;
    case Operation::GetParamsBatch:
        connect(resource, &QnResource::asyncParamsGetDone, this, &QnCameraSettingsRestHandler::asyncParamsGetComplete, Qt::DirectConnection);
        break;
    case Operation::SetParam:
        connect(resource, &QnResource::asyncParamSetDone, this, &QnCameraSettingsRestHandler::asyncParamSetComplete, Qt::DirectConnection);
        break;
    case Operation::SetParamsBatch:
        connect(resource, &QnResource::asyncParamsSetDone, this, &QnCameraSettingsRestHandler::asyncParamsSetComplete, Qt::DirectConnection);
        break;
    }
}

void QnCameraSettingsRestHandler::disconnectFromResource(const QnResourcePtr &resource, Operation operation) {
    switch (operation) {
    case Operation::GetParam:
        disconnect(resource, &QnResource::asyncParamGetDone, this, &QnCameraSettingsRestHandler::asyncParamGetComplete);
        break;
    case Operation::GetParamsBatch:
        disconnect(resource, &QnResource::asyncParamsGetDone, this, &QnCameraSettingsRestHandler::asyncParamsGetComplete);
        break;
    case Operation::SetParam:
        disconnect(resource, &QnResource::asyncParamSetDone, this, &QnCameraSettingsRestHandler::asyncParamSetComplete);
        break;
    case Operation::SetParamsBatch:
        disconnect(resource, &QnResource::asyncParamsSetDone, this, &QnCameraSettingsRestHandler::asyncParamsSetComplete);
        break;
    }
}

void QnCameraSettingsRestHandler::processOperation(const QnResourcePtr &resource, Operation operation, const QnCameraAdvancedParamValueList &values) {
    switch (operation) {
    case Operation::GetParam:
        for (const QnCameraAdvancedParamValue value: values)
            resource->getParamPhysicalAsync(value.id);
        break;
    case Operation::GetParamsBatch:
        {
            QSet<QString> ids;
            for (const QnCameraAdvancedParamValue value: values)
                ids.insert(value.id);
            resource->getParamsPhysicalAsync(ids);
        }
        break;
    case Operation::SetParam:
        for (const QnCameraAdvancedParamValue value: values)
            resource->setParamPhysicalAsync(value.id, value.value);
        break;
    case Operation::SetParamsBatch:
        resource->setParamsPhysicalAsync(values);
        break;
    }
}

void QnCameraSettingsRestHandler::asyncParamGetComplete(const QnResourcePtr &resource, const QString &id, const QString &value, bool success) {
    QMutexLocker lk( &m_mutex );

    NX_LOG( QString::fromLatin1("QnCameraSettingsHandler::asyncParamGetComplete. paramName %1, paramValue %2").arg(id).arg(value), cl_logDEBUG1 );
    for( std::set<AwaitedParameters*>::const_iterator
        it = m_awaitedParamsSets.begin();
        it != m_awaitedParamsSets.end();
        ++it )
    {
		auto awaitedParams = (*it);
        if(!awaitedParams || awaitedParams->resource != resource)
            continue;

		if (!awaitedParams->requested.contains(id))
			continue;

		if (success)
			awaitedParams->result << QnCameraAdvancedParamValue(id, value);

        awaitedParams->requested.remove(id);
    }

    m_cond.wakeAll();
}

void QnCameraSettingsRestHandler::asyncParamSetComplete(const QnResourcePtr &resource, const QString &id, const QString &value, bool success) {
    //processing is identical to the previous method
    asyncParamGetComplete(resource, id, value, success);
}

void QnCameraSettingsRestHandler::asyncParamsGetComplete(const QnResourcePtr &resource, const QnCameraAdvancedParamValueList &values) {
    QMutexLocker lk( &m_mutex );

    for( std::set<AwaitedParameters*>::const_iterator
        it = m_awaitedParamsSets.begin();
        it != m_awaitedParamsSets.end();
    ++it )
    {
        auto awaitedParams = (*it);
        if(!awaitedParams || awaitedParams->resource != resource)
            continue;

        for (const QnCameraAdvancedParamValue &value: values) {
            if (!awaitedParams->requested.contains(value.id))
                 continue;
            awaitedParams->result << value;
        }
        awaitedParams->requested.clear();
    }
    m_cond.wakeAll();
}

void QnCameraSettingsRestHandler::asyncParamsSetComplete(const QnResourcePtr &resource, const QnCameraAdvancedParamValueList &values) {
    //processing is identical to the previous method
    asyncParamsGetComplete(resource, values);
}



