#pragma once

#include <nx/utils/log/log_main.h>

struct QnLogs;
struct QnLog
{
    static NX_UTILS_API const nx::utils::log::Tag MAIN_LOG_ID;
    static NX_UTILS_API const nx::utils::log::Tag HTTP_LOG_INDEX;
    static NX_UTILS_API const nx::utils::log::Tag EC2_TRAN_LOG;
    static NX_UTILS_API const nx::utils::log::Tag HWID_LOG;
    static NX_UTILS_API const nx::utils::log::Tag PERMISSIONS_LOG;

    static NX_UTILS_API void initLog(QnLog* log);
    static NX_UTILS_API QnLogs& instance();
};

struct QnLogs
{
    static NX_UTILS_API std::vector<QString> getLoggerNames();
    static NX_UTILS_API std::shared_ptr<nx::utils::log::AbstractLogger> getLogger(int id);
    static NX_UTILS_API std::shared_ptr<nx::utils::log::AbstractLogger> getLogger(const QString& name);
    static NX_UTILS_API QnLog* get();
};

