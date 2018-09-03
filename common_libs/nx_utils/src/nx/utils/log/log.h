#pragma once

#include <nx/utils/log/log_main.h>

typedef nx::utils::log::Level QnLogLevel;
static constexpr const QnLogLevel cl_logUNKNOWN = nx::utils::log::Level::undefined;
static constexpr const QnLogLevel cl_logNONE = nx::utils::log::Level::none;
static constexpr const QnLogLevel cl_logALWAYS = nx::utils::log::Level::always;
static constexpr const QnLogLevel cl_logERROR = nx::utils::log::Level::error;
static constexpr const QnLogLevel cl_logWARNING = nx::utils::log::Level::warning;
static constexpr const QnLogLevel cl_logINFO = nx::utils::log::Level::info;
static constexpr const QnLogLevel cl_logDEBUG1 = nx::utils::log::Level::debug;
static constexpr const QnLogLevel cl_logDEBUG2 = nx::utils::log::Level::verbose;

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

