#include "settime_rest_handler.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/time.h>

#include <api/app_server_connection.h>
#include <network/tcp_connection_priv.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <rest/server/rest_connection_processor.h>

struct SetTimeData
{
    SetTimeData() {}

    /**
     * Used for the deprecated GET method (the new one is POST).
     */
    SetTimeData(const QnRequestParams& params):
        dateTime(params.value("datetime")),
        timeZoneId(params.value("timezone"))
    {
    }

    QString dateTime;
    QString timeZoneId;
};

#define SetTimeData_Fields (dateTime)(timeZoneId)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((SetTimeData), (json), _Fields)

/**
 * GET is the deprecated API method.
 */
int QnSetTimeRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    return execute(SetTimeData(params), owner, result);
}

int QnSetTimeRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    SetTimeData data = QJson::deserialized<SetTimeData>(body);
    return execute(data, owner, result);
}

int QnSetTimeRestHandler::execute(
    const SetTimeData& data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult& result)
{
    QnMediaServerResourcePtr mServer = owner->resourcePool()->getResourceById<QnMediaServerResource>(
        owner->commonModule()->moduleGUID());
    if (!mServer)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error"));
        return CODE_OK;
    }
    if (!(mServer->getServerFlags() & Qn::SF_timeCtrl))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest, lit("This server does not support time control"));
        return CODE_OK;
    }

    // NOTE: Setting the time zone should be done before converting date-time from the
    // formatted string, because the convertion depends on the current time zone.
    if (!nx::utils::setTimeZone(data.timeZoneId))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest, lit("Invalid time zone specified"));
        return CODE_OK;
    }

    qint64 dateTime = -1;
    if (data.dateTime.toLongLong() > 0)
    {
        dateTime = data.dateTime.toLongLong();
    }
    else
    {
        dateTime = QDateTime::fromString(data.dateTime, lit("yyyy-MM-ddThh:mm:ss"))
            .toMSecsSinceEpoch();
    }
    if (dateTime < 1)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest, lit("Invalid date-time format specified"));
        return CODE_OK;
    }

    if (!nx::utils::setDateTime(dateTime))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest, lit("Can't set new date-time value"));
        return CODE_OK;
    }

    //ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();
    //ec2Connection->getTimeManager()->forceTimeResync();

    return CODE_OK;
}
