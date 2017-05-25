#include "log.h"

const QString QnLog::MAIN_LOG_ID = QLatin1String();
const QString QnLog::CUSTOM_LOG_BASE_ID = QLatin1String("CUSTOM");
const QString QnLog::HTTP_LOG_INDEX = QLatin1String("HTTP");
const QString QnLog::EC2_TRAN_LOG = QLatin1String("EC2_TRAN");
const QString QnLog::HWID_LOG = QLatin1String("HWID");
const QString QnLog::PERMISSIONS_LOG = QLatin1String("PERMISSIONS");

void QnLog::initLog(QnLog* /*log*/)
{
    // No need to initialize any more.
}

QnLogs& QnLog::instance()
{
    static QnLogs logs;
    return logs;
}

std::shared_ptr<nx::utils::log::Logger> QnLogs::logger(int id)
{
    if (id == 0)
        return nx::utils::log::mainLogger();

    static std::vector<QString> kAllLogs =
    {
        QnLog::MAIN_LOG_ID,
        QnLog::CUSTOM_LOG_BASE_ID,
        QnLog::HTTP_LOG_INDEX,
        QnLog::EC2_TRAN_LOG,
        QnLog::HWID_LOG,
        QnLog::PERMISSIONS_LOG,
    };

    if (id < 0 || (size_t) id >= kAllLogs.size())
        return nullptr;

    return nx::utils::log::getLogger(kAllLogs[id], /*allowMain*/ false);
}

QnLog* QnLogs::get()
{
    // No need to use any more.
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
