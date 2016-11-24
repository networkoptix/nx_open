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

    SetTimeData(const QnRequestParams& params):
        datetime(params.value("datetime")),
        timezone(params.value("timezone"))
    {
    }

    QString datetime;
    QString timezone;
};

#define SetTimeData_Fields (datetime)(timezone)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
(SetTimeData),
(json),
_Fields)

#ifdef Q_OS_LINUX
#include <sys/time.h>

bool setTimeZone(const QString& timezone)
{
    QString timezoneFile = QString(lit("/usr/share/zoneinfo/%1")).arg(timezone);
    if (!QFile::exists(timezoneFile))
        return false;
    if (unlink("/etc/localtime") != 0)
        return false;
    if (symlink(timezoneFile.toLatin1().data(), "/etc/localtime") != 0)
        return false;
    QFile tzFile(lit("/etc/timezone"));
    if (!tzFile.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    return tzFile.write(timezone.toLatin1()) != 0;
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
#endif

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
    qint64 dateTime = -1;
    if (data.datetime.toLongLong() > 0)
        dateTime = data.datetime.toLongLong();
    else
        dateTime = QDateTime::fromString(data.datetime, QLatin1String("yyyy-MM-ddThh:mm:ss")).toMSecsSinceEpoch();


    if (dateTime < 1) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Invalid datetime format specified"));
        return CODE_OK;
    }

    QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!mServer) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error"));
        return CODE_OK;
    }
    if (!(mServer->getServerFlags() & Qn::SF_timeCtrl)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("This server doesn't support time control"));
        return CODE_OK;
    }

#ifdef Q_OS_LINUX
    if (!data.timezone.isEmpty()) {
        if (!setTimeZone(data.timezone)) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("Invalid timezone specified"));
            return CODE_OK;
        }
    }

    if (!setDateTime(dateTime)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't set new datetime value"));
        return CODE_OK;
    }

#endif

    //ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    //ec2Connection->getTimeManager()->forceTimeResync();

    return CODE_OK;
}
