#include "log_level_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>

#include <utils/common/log.h>

namespace {
    const QString idParam = lit("id");
    const QString valueParam = lit("value");

    const QString disableLogsOption = lit("none");
}


int QnLogLevelRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *processor) {
    QN_UNUSED(path, processor);

    int logID = QnLog::MAIN_LOG_ID;
    requireParameter(params, idParam, result, &logID, true);

    if (!QnLog::instanceExists(logID)) {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Parameter '%1' has invalid value '%2'").arg(idParam).arg(logID));
        return CODE_INVALID_PARAMETER;
    }

    auto setValue = params.find(valueParam);
    if (setValue != params.cend()) {
        QString level = *setValue;

        QnLogLevel logLevel = QnLog::logLevelFromString(level);
        if (logLevel == cl_logUNKNOWN 
            && level.toLower() != disableLogsOption //TODO: #ak add cl_logNONE level to disable logs
            ) {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Parameter '%1' has invalid value '%2'").arg(valueParam).arg(level));
            return CODE_INVALID_PARAMETER;
        }

        QnLog::instance(logID)->setLogLevel(logLevel);
    }
            
    QnLogLevel resultLevel = QnLog::instance(logID)->logLevel();
    result.setReply(resultLevel == cl_logUNKNOWN
        ? disableLogsOption.toUpper() // to maintain common style
        : QnLog::logLevelToString(resultLevel));

    return CODE_OK;
}
