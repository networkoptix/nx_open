#include "log.h"

static QString kMainLogName("MAIN");
const nx::utils::log::Tag QnLog::MAIN_LOG_ID(QLatin1String(""));
const nx::utils::log::Tag QnLog::HTTP_LOG_INDEX(QLatin1String("HTTP"));
const nx::utils::log::Tag QnLog::EC2_TRAN_LOG(QLatin1String("EC2_TRAN"));
const nx::utils::log::Tag QnLog::HWID_LOG(QLatin1String("HWID"));
const nx::utils::log::Tag QnLog::PERMISSIONS_LOG(QLatin1String("PERMISSIONS"));

void QnLog::initLog(QnLog* /*log*/)
{
    // No need to initialize any more.
}

QnLogs& QnLog::instance()
{
    static QnLogs logs;
    return logs;
}

std::vector<QString> QnLogs::getLoggerNames()
{
    return
    {
        kMainLogName,
        QnLog::HTTP_LOG_INDEX.toString(),
        QnLog::EC2_TRAN_LOG.toString(),
        QnLog::HWID_LOG.toString(),
        QnLog::PERMISSIONS_LOG.toString(),
    };
}

std::shared_ptr<nx::utils::log::AbstractLogger> QnLogs::getLogger(int id)
{
    // Currently hardcoded in some places of desktop client.
    switch (id)
    {
        case 0: return nx::utils::log::mainLogger();
        case 1: return nx::utils::log::getExactLogger(QnLog::HTTP_LOG_INDEX);
        case 2: return nx::utils::log::getExactLogger(QnLog::HWID_LOG);
        case 3: return nx::utils::log::getExactLogger(QnLog::EC2_TRAN_LOG);
        case 4: return nx::utils::log::getExactLogger(QnLog::PERMISSIONS_LOG);
        default: return nullptr;
    }
}

std::shared_ptr<nx::utils::log::AbstractLogger> QnLogs::getLogger(const QString& name)
{
    if (name == kMainLogName)
        return nx::utils::log::mainLogger();

    return nx::utils::log::getExactLogger(nx::utils::log::Tag(name));
}

QnLog* QnLogs::get()
{
    // No need to use any more.
    return nullptr;
}
