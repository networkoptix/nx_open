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
    static const QString MAIN_LOG_ID;
    static const QString CUSTOM_LOG_BASE_ID;
    static const QString HTTP_LOG_INDEX;
    static const QString EC2_TRAN_LOG;
    static const QString HWID_LOG;
    static const QString PERMISSIONS_LOG;

    static std::vector<QString> kAllLogs;

    static void initLog(QnLog* log);
    static QnLogs& instance();
};

struct QnLogs
{
    static QnLog* get();
};

#define NX_LOG_2(MESSAGE, LEVEL) NX_UTILS_LOG(LEVEL, QnLog::MAIN_LOG_ID, MESSAGE)
#define NX_LOG_3(ID, MESSAGE, LEVEL) NX_UTILS_LOG(LEVEL, ID, MESSAGE)

#define NX_LOGX_2(MESSAGE, LEVEL) NX_UTILS_LOG(LEVEL, this, MESSAGE)
#define NX_LOGX_3(ID, MESSAGE, LEVEL) NX_UTILS_LOG(LEVEL, lm("%1::%2").strs(ID, this), MESSAGE);

#define GET_NX_LOG_MACRO(_1, _2, _3, NAME, ...) NAME
#define NX_LOG(...) NX_MSVC_EXPAND(GET_NX_LOG_MACRO(__VA_ARGS__, NX_LOG_3, NX_LOG_2)(__VA_ARGS__))
#define NX_LOGX(...) NX_MSVC_EXPAND(GET_NX_LOG_MACRO(__VA_ARGS__, NX_LOGX_3, NX_LOGX_2)(__VA_ARGS__))

void NX_UTILS_API qnLogMsgHandler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg);
