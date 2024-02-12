// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/analytics_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractAnalyticsNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void analyticsPluginAddedOrUpdated(
        const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
        ec2::NotificationSource source);

    void analyticsEngineAddedOrUpdated(
        const nx::vms::api::AnalyticsEngineData& analyticsEngine,
        ec2::NotificationSource source);

    void analyticsPluginRemoved(const nx::Uuid& id, ec2::NotificationSource source);

    void analyticsEngineRemoved(const nx::Uuid& id, ec2::NotificationSource source);
};

/**
 * NOTE: All methods are asynchronous, unless specified otherwise.
 */
class NX_VMS_COMMON_API AbstractAnalyticsManager
{
public:
    virtual ~AbstractAnalyticsManager() = default;

    virtual int getAnalyticsPlugins(
        Handler<nx::vms::api::AnalyticsPluginDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getAnalyticsPluginsSync(
        nx::vms::api::AnalyticsPluginDataList* outDataList);

    virtual int getAnalyticsEngines(
        Handler<nx::vms::api::AnalyticsEngineDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getAnalyticsEnginesSync(
        nx::vms::api::AnalyticsEngineDataList* outDataList);

    virtual int save(
        const nx::vms::api::AnalyticsPluginData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::AnalyticsPluginData& data);

    virtual int save(
        const nx::vms::api::AnalyticsEngineData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::AnalyticsEngineData& data);

    virtual int removeAnalyticsPlugin(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeAnalyticsPluginSync(const nx::Uuid& id);

    virtual int removeAnalyticsEngine(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeAnalyticsEngineSync(const nx::Uuid& id);
};

} // namespace ec2
