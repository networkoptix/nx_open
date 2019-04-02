#include "log_level_rest_handler.h"

#include <network/tcp_connection_priv.h>

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>
#include <core/resource_access/resource_access_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

using namespace nx::utils::log;
using namespace nx::network;

static const QString kNameParam = lit("name");
static const QString kIdParam = lit("id");
static const QString kValueParam = lit("value");

static bool hasPermission(const QnRestConnectionProcessor* processor)
{
    return processor->commonModule()->resourceAccessManager()->hasGlobalPermission(
        processor->accessRights(), GlobalPermission::admin);
}

static QString levelString(const std::shared_ptr<AbstractLogger>& logger)
{
    LevelSettings settings(logger->defaultLevel(), logger->levelFilters());
    return settings.toString();
}

rest::Response QnLogLevelRestHandler::executeGet(const rest::Request& request)
{
    // TODO: Remove as soon as WEB client supports new API.
    if (request.param(kIdParam))
        return manageLogLevelById(request);

    if (request.params().empty())
    {
        std::map<QString, QString> levelsByName;
        for (const auto& name: QnLogs::getLoggerNames())
            levelsByName[name] = levelString(QnLogs::getLogger(name));
        return rest::Response::reply(levelsByName);
    }

    const auto name = request.paramOr(kNameParam);
    const auto logger = QnLogs::getLogger(name);
    if (!logger)
        return rest::Response::error(QnRestResult::InvalidParameter, kNameParam, name);

    const auto value = request.paramOr(kValueParam);
    if (!value.isEmpty())
    {
        if (!hasPermission(request.owner))
            return rest::Response::error(QnRestResult::Forbidden);

        LevelSettings settings;
        if (!settings.parse(value))
            return rest::Response::error(QnRestResult::InvalidParameter, kValueParam, value);

        // TODO: Also update in config.
        logger->setDefaultLevel(settings.primary);
        logger->setLevelFilters(settings.filters);
    }

    return rest::Response::reply(levelString(logger));
}

rest::Response QnLogLevelRestHandler::manageLogLevelById(const rest::Request& request)
{
    std::shared_ptr<nx::utils::log::AbstractLogger> logger;
    const auto logId = request.paramOr(kIdParam);
    {
        bool isLogIdInt = false;
        const auto logIdInt = logId.toInt(&isLogIdInt);
        if (isLogIdInt)
            logger = QnLogs::getLogger(logIdInt);
    }

    if (!logger)
        return rest::Response::error(QnRestResult::InvalidParameter, kIdParam, logId);

    auto value = request.params().find(kValueParam);
    if (value != request.params().cend())
    {
        if (!hasPermission(request.owner))
            return rest::Response::error(QnRestResult::Forbidden);

        const auto logLevel = nx::utils::log::levelFromString(*value);
        if (logLevel == Level::undefined)
            return rest::Response::error(QnRestResult::InvalidParameter, kValueParam, *value);

        logger->setDefaultLevel(logLevel);
    }

    const auto level = logger->defaultLevel();
    return rest::Response::reply(
        (level == Level::verbose) ? lit("DEBUG2") : toString(level).toUpper());
}
