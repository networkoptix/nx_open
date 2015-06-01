#include <QFile>

#include "settime_rest_handler.h"
#include <utils/network/tcp_connection_priv.h>
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

#ifdef Q_OS_LINUX
#include <sys/time.h>

void setTimeZone(const QString& timezone)
{
    QString tz = QString(lit(":%1")).arg(timezone);
    setenv("TZ", tz.toLatin1().data(), 1);
    tzset();
}

void setDateTime(const QDateTime& datetime)
{
    struct timeval tv;
    tv.tv_sec = datetime.toMSecsSinceEpoch() / 1000;
    tv.tv_usec = (datetime.toMSecsSinceEpoch() % 1000) * 1000;
    settimeofday(&tv, 0);
}
#endif

int QnSetTimeRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)
    QString timezone = params.value("timezone");
    QString dateTimeStr = params.value("datetime");
    QDateTime dateTime;
    if (dateTimeStr.toLongLong() > 0)
        dateTime = QDateTime::fromMSecsSinceEpoch(dateTimeStr.toLongLong());
    else
        dateTime = QDateTime::fromString(dateTimeStr, QLatin1String("yyyy-MM-ddThh:mm:ss"));
    

    if (!dateTime.isValid()) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("Invalid datetime format specified"));
        return CODE_OK;
    }

    QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!mServer) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("Internal server error"));
        return CODE_OK;
    }
    if (!(mServer->getServerFlags() & Qn::SF_timeCtrl)) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("This server doesn't support time control"));
        return CODE_OK;
    }
    
#ifdef Q_OS_LINUX
    if (!timezone.isEmpty())
        setTimeZone(timezone);
    setDateTime(dateTime);
#endif
    return CODE_OK;
}
