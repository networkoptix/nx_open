#include "log_level_rest_handler.h"

#include <network/tcp_connection_priv.h>

#include <nx/utils/log/log.h>

namespace {
    const QString idParam = lit("id");
    const QString valueParam = lit("value");
}

int QnLogLevelRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *processor) {
    QN_UNUSED(path, processor);

    int logID = QnLog::MAIN_LOG_ID;
    requireParameter(params, idParam, result, &logID, true);

    auto _nx_logs = QnLog::logs();
    if (!_nx_logs)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Logging subsystem is not initialized"));
        return CODE_OK;
    }

    if (!_nx_logs->exists(logID)) {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Parameter '%1' has invalid value '%2'").arg(idParam).arg(logID));
        return CODE_OK;
    }

    auto setValue = params.find(valueParam);
    if (setValue != params.cend()) {
        QString level = *setValue;

        QnLogLevel logLevel = QnLog::logLevelFromString(level);
        if (logLevel == cl_logUNKNOWN)
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Parameter '%1' has invalid value '%2'").arg(valueParam).arg(level));
            return CODE_OK;
        }

        QnLog::instance(logID)->setLogLevel(logLevel);
    }
            
    const QnLogLevel resultLevel = QnLog::instance(logID)->logLevel();
    result.setReply(QnLog::logLevelToString(resultLevel));

    return CODE_OK;
}
