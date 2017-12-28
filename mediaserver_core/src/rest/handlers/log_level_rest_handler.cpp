#include "log_level_rest_handler.h"

#include <network/tcp_connection_priv.h>

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>
#include <core/resource_access/resource_access_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

namespace {
    const QString idParam = lit("id");
    const QString valueParam = lit("value");
}

int QnLogLevelRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor *processor)
{
    QN_UNUSED(path, processor);

    int logId = 0;
    requireParameter(params, idParam, result, &logId, true);
    const auto logger = QnLogs::logger(logId);
    if (!logger)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Parameter '%1' has invalid value '%2'").arg(idParam).arg(logId));
        return CODE_OK;
    }

    auto setValue = params.find(valueParam);
    if (setValue != params.cend())
    {
        if (!processor->commonModule()->resourceAccessManager()->hasGlobalPermission(
                processor->accessRights(),
                Qn::GlobalPermission::GlobalAdminPermission))
        {
            result.setError(QnJsonRestResult::Forbidden);
            return nx::network::http::StatusCode::forbidden;
        }

        QString level = *setValue;
        QnLogLevel logLevel = nx::utils::log::levelFromString(level);
        if (logLevel == cl_logUNKNOWN)
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Parameter '%1' has invalid value '%2'").arg(valueParam).arg(level));
            return CODE_OK;
        }

        logger->setDefaultLevel(logLevel);
    }

    const auto level = logger->defaultLevel();
    result.setReply(level == nx::utils::log::Level::verbose ? lit("DEBUG2") : toString(level).toUpper());
    return CODE_OK;
}
