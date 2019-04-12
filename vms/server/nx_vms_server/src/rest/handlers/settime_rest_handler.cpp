#include "settime_rest_handler.h"

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/time.h>
#include <rest/server/rest_connection_processor.h>

namespace nx::vms::server {

struct SetTimeData
{
    QString dateTime;
    QString timeZoneId;
};

#define SetTimeData_Fields (dateTime)(timeZoneId)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((SetTimeData), (json), _Fields)

nx::network::rest::Response SetTimeRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowGetModifications)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    const SetTimeData data{request.paramOrDefault("datetime"), request.paramOrDefault("datetime")};
    return execute(data, request.owner);
}

nx::network::rest::Response SetTimeRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    return execute(request.parseContentOrThrow<SetTimeData>(), request.owner);
}

static nx::network::rest::Response cantProcess(const QString& message)
{
    return nx::network::rest::Response::error(
        nx::network::http::StatusCode::ok, nx::network::rest::Result::CantProcessRequest, message);
}

nx::network::rest::Response SetTimeRestHandler::execute(
    const SetTimeData& data,
    const QnRestConnectionProcessor* owner)
{
    QnMediaServerResourcePtr mServer = owner->resourcePool()->getResourceById<QnMediaServerResource>(
        owner->commonModule()->moduleGUID());

    if (!mServer)
        return cantProcess("Internal server error");

    if (!mServer->getServerFlags().testFlag(nx::vms::api::SF_timeCtrl))
        return cantProcess("This server does not support time control");

    // NOTE: Setting the time zone should be done before converting date-time from the
    // formatted string, because the convertion depends on the current time zone.
    if (!nx::utils::setTimeZone(data.timeZoneId))
        return cantProcess("Invalid time zone specified");

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
        return cantProcess("Invalid date-time format specified");

    if (!nx::utils::setDateTime(dateTime))
        return cantProcess("Can't set new date-time value");

    return nx::network::rest::Response::result(nx::network::rest::JsonResult());
}

} // namespace nx::vms::server
