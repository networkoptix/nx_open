#include <QFile>

#include "settime_rest_handler.h"
#include <utils/network/tcp_connection_priv.h>
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

#ifdef Q_OS_LINUX
#include <sys/time.h>

bool setTimeZone(const QString& timezone)
{
    QString timezoneFile = QString(lit("/usr/share/zoneinfo/%1")).arg(timezone);
    if (!QFile::exists(timezoneFile))
	return false;
    if (unlink("/etc/localtime") != 0)
	return false;
    return symlink(timezoneFile.toLatin1().data(), "/etc/localtime") == 0;
}

bool setDateTime(const QDateTime& datetime)
{
    struct timeval tv;
    tv.tv_sec = datetime.toMSecsSinceEpoch() / 1000;
    tv.tv_usec = (datetime.toMSecsSinceEpoch() % 1000) * 1000;
    return settimeofday(&tv, 0) == 0;
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
    if (!timezone.isEmpty()) {
        if (!setTimeZone(timezone)) {
    	    result.setError(QnJsonRestResult::CantProcessRequest);
    	    result.setErrorString(lit("Invalid timezone specified"));
    	    return CODE_OK;
	}
    }
    if (!setDateTime(dateTime)) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("Can't set new datetime value"));
        return CODE_OK;
    }


#endif
    return CODE_OK;
}
