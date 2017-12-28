#include "log_level_rest_handler.h"

#include <network/tcp_connection_priv.h>

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>
#include <core/resource_access/resource_access_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

using namespace nx::utils::log;

static const QString kNameParam = lit("name");
static const QString kIdParam = lit("id");
static const QString kValueParam = lit("value");

static bool hasPermission(const QnRestConnectionProcessor* processor)
{
    return processor->commonModule()->resourceAccessManager()->hasGlobalPermission(
        processor->accessRights(), Qn::GlobalPermission::GlobalAdminPermission);
}

static QString levelString(const std::shared_ptr<Logger>& logger)
{
    LevelSettings settings(logger->defaultLevel(), logger->levelFilters());
    return settings.toString();
}

static JsonRestResponse invalidParamiter(const QString& name, const QString& value)
{
    return {QnJsonRestResult::InvalidParameter,
        lm("Parameter '%1' has invalid value '%2'").args(name, value)};
}

JsonRestResponse QnLogLevelRestHandler::executeGet(const JsonRestRequest& request)
{
    // TODO: Remove as soon as WEB client supports new API.
    if (request.params.contains(kIdParam))
        return manageLogLevelById(request);
    
    if (request.params.empty())
    {
        std::map<QString, QString> levelsByName;
        for (const auto& name: QnLogs::getLoggerNames())
            levelsByName[name] = levelString(QnLogs::getLogger(name));
        return {levelsByName};
    }

    const auto name = request.params.value(kNameParam);
    const auto logger = QnLogs::getLogger(name);
    if (!logger)
        return invalidParamiter(kNameParam, name);

    const auto value = request.params.value(kValueParam);
    if (!value.isEmpty())
    {
        if (!hasPermission(request.owner))
            return {nx_http::StatusCode::forbidden, QnJsonRestResult::Forbidden};

        LevelSettings settings;
        if (!settings.parse(value))
            return invalidParamiter(kValueParam, value);

        // TODO: Also update in config.
        logger->setDefaultLevel(settings.primary);
        logger->setLevelFilters(settings.filters);
    }
    
    return {levelString(logger)};
}

JsonRestResponse QnLogLevelRestHandler::manageLogLevelById(const JsonRestRequest& request)
{
    std::shared_ptr<nx::utils::log::Logger> logger;
    const auto logId = request.params.value(kIdParam);
    {
        bool isLogIdInt = false;
        const auto logIdInt = logId.toInt(&isLogIdInt);
        if (isLogIdInt)
            logger = QnLogs::getLogger(logIdInt);
    }

    if (!logger)
        return invalidParamiter(kIdParam, logId);

    auto value = request.params.find(kValueParam);
    if (value != request.params.cend())
    {
        if (!hasPermission(request.owner))
            return {nx_http::StatusCode::forbidden, QnJsonRestResult::Forbidden};

        const auto logLevel = nx::utils::log::levelFromString(*value);
        if (logLevel == Level::undefined)
            return invalidParamiter(kValueParam, *value);

        logger->setDefaultLevel(logLevel);
    }

    const auto level = logger->defaultLevel();
    return {(level == Level::verbose) ? lit("DEBUG2") : toString(level).toUpper()};
}
