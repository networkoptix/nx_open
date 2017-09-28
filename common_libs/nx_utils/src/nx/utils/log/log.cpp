#include "log.h"

const nx::utils::log::Tag QnLog::MAIN_LOG_ID(lit(""));
const nx::utils::log::Tag QnLog::CUSTOM_LOG_BASE_ID(lit("CUSTOM"));
const nx::utils::log::Tag QnLog::HTTP_LOG_INDEX(lit("HTTP"));
const nx::utils::log::Tag QnLog::EC2_TRAN_LOG(lit("EC2_TRAN"));
const nx::utils::log::Tag QnLog::HWID_LOG(lit("HWID"));
const nx::utils::log::Tag QnLog::PERMISSIONS_LOG(lit("PERMISSIONS"));

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

    static std::vector<nx::utils::log::Tag> kAllLogs =
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
    // TODO: #Elric use ctx

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
