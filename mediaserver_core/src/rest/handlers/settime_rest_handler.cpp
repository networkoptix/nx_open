#include <QFile>
#include <QtCore/QProcess>

#include <nx/fusion/model_functions.h>

#include "settime_rest_handler.h"
#include <api/app_server_connection.h>
#include <network/tcp_connection_priv.h>
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

#include <utils/common/app_info.h>

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

#if defined(Q_OS_LINUX)

#include <sys/time.h>

bool setTimeZone(const QString& timeZoneId)
{
    QString timeZoneFile = QString(lit("/usr/share/zoneinfo/%1")).arg(timeZoneId);
    if (!QFile::exists(timeZoneFile))
        return false;
    if (unlink("/etc/localtime") != 0)
        return false;
    if (symlink(timeZoneFile.toLatin1().data(), "/etc/localtime") != 0)
        return false;
    QFile tzFile(lit("/etc/timezone"));
    if (!tzFile.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    return tzFile.write(timeZoneId.toLatin1()) != 0;
}


bool setDateTime(qint64 value)
{
    struct timeval tv;
    tv.tv_sec = value / 1000;
    tv.tv_usec = (value % 1000) * 1000;
    if (settimeofday(&tv, 0) != 0)
        return false;

    if (QnAppInfo::isBpi() || QnAppInfo::isNx1())
    {
        //on nx1 have to execute hwclock -w to save time. But this command sometimes failes
        for (int i = 0; i < 3; ++i)
        {
            const int resultCode = QProcess::execute("hwclock -w");
            if (resultCode == 0)
                return true;
        }
        return false;
    }

    return true;

}

#endif // defined(Q_OS_LINUX)

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
    const QnRestConnectionProcessor* /*owner*/,
    QnJsonRestResult& result)
{
    #if defined(Q_OS_LINUX)
        // NOTE: Setting the time zone should be done before converting date-time from the
        // formatted string, because the convertion depends on the current time zone.
        if (!data.timeZoneId.isEmpty())
        {
            if (!setTimeZone(data.timeZoneId))
            {
                result.setError(
                    QnJsonRestResult::CantProcessRequest, lit("Invalid time zone specified"));
                return CODE_OK;
            }
        }
    #endif // defined(Q_OS_LINUX)

    qint64 dateTime = -1;
    if (data.dateTime.toLongLong() > 0)
    {
        dateTime = data.dateTime.toLongLong();
    }
    else
    {
        dateTime = QDateTime::fromString(
            data.dateTime, QLatin1String("yyyy-MM-ddThh:mm:ss")).toMSecsSinceEpoch();
    }
    if (dateTime < 1)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest, lit("Invalid date-time format specified"));
        return CODE_OK;
    }

    QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(
        qnCommon->moduleGUID());
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

    #if defined(Q_OS_LINUX)
        if (!setDateTime(dateTime))
        {
            result.setError(
                QnJsonRestResult::CantProcessRequest, lit("Can't set new date-time value"));
            return CODE_OK;
        }
    #endif // defined(Q_OS_LINUX)

    //ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    //ec2Connection->getTimeManager()->forceTimeResync();

    return CODE_OK;
}
