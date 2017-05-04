#include "log.h"

const QString QnLog::MAIN_LOG_ID = QLatin1String();
const QString QnLog::CUSTOM_LOG_BASE_ID = QLatin1String("CUSTOM");
const QString QnLog::HTTP_LOG_INDEX = QLatin1String("HTTP");
const QString QnLog::EC2_TRAN_LOG = QLatin1String("EC2_TRAN");
const QString QnLog::P2P_TRAN_LOG = QLatin1String("P2P_TRAN");
const QString QnLog::HWID_LOG = QLatin1String("HWID");
const QString QnLog::PERMISSIONS_LOG = QLatin1String("PERMISSIONS");

std::vector<QString> QnLog::kAllLogs =
{
    QnLog::MAIN_LOG_ID,
    QnLog::CUSTOM_LOG_BASE_ID,
    QnLog::HTTP_LOG_INDEX,
    QnLog::EC2_TRAN_LOG,
    QnLog::P2P_TRAN_LOG,
    QnLog::HWID_LOG,
    QnLog::PERMISSIONS_LOG,
};

void QnLog::initLog(QnLog* /*log*/)
{
    // No need to initialize any more.
}

QnLogs& QnLog::instance()
{
    static QnLogs logs;
    return logs;
}

QnLog* QnLogs::get()
{
    return nullptr;
}

void qnLogMsgHandler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg)
{
    //TODO: #Elric use ctx

    QnLogLevel logLevel;
    switch (type)
    {
        case QtFatalMsg:
        case QtCriticalMsg:
            logLevel = cl_logERROR;
            break;
        case QtWarningMsg:
            logLevel = cl_logWARNING;
            break;
        case QtDebugMsg:
            logLevel = cl_logDEBUG1;
            break;
        default:
            logLevel = cl_logINFO;
            break;
    }

    NX_LOG(msg, logLevel);
}
