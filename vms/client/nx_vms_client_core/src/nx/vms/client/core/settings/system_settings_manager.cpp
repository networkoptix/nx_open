// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings_manager.h"

#include <chrono>
#include <list>
#include <optional>

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/system_context.h>

using namespace std::chrono;

namespace {

static constexpr auto kRequestDataRetryTimeout = 30s;

} // namespace

namespace nx::vms::client::core {

using namespace nx::vms::api;

struct SystemSettingsManager::Private
{
    SystemSettingsManager* q = nullptr;
    std::unique_ptr<SystemSettings> systemSettings = std::make_unique<SystemSettings>();
    QTimer* requestDataTimer = nullptr;

    void requestSystemSettings(std::function<void()> callback = {})
    {
        const auto handle = nx::utils::guarded(q,
            [this, callback](bool success,
                rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders)
            {
                if (!success)
                {
                     NX_WARNING(this, "System settings request failed");
                     return;
                }

                nx::reflect::DeserializationResult deserializationResult;
                SystemSettings result;

                std::tie(result, deserializationResult) =
                   nx::reflect::json::deserialize<SystemSettings>(data.data());
                if (!deserializationResult.success)
                {
                    NX_WARNING(this, "Server settings cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "System settings received");

                auto hasChange = *systemSettings != result;
                if (hasChange)
                    *systemSettings = result;

                if (callback)
                    callback();

                if (hasChange)
                    emit q->systemSettingsChanged();
            }
        );

        auto api = q->connectedServerApi();
        if (!NX_ASSERT(api))
            return;

        api->getRawResult(
            "/rest/v3/system/settings",
            {},
            handle,
            q->thread());
    }
};

SystemSettingsManager::SystemSettingsManager(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d(new Private{.q = this})
{
    d->requestDataTimer = new QTimer(this);
    d->requestDataTimer->setInterval(kRequestDataRetryTimeout);
    d->requestDataTimer->callOnTimeout([this]() { d->requestSystemSettings(); });

    connect(systemContext(),
        &SystemContext::remoteIdChanged,
        this,
        [this](const QnUuid& id)
        {
            if (id.isNull())
            {
                d->requestDataTimer->stop();
                return;
            }
            d->requestDataTimer->start();
            d->requestSystemSettings();
        });
}

SystemSettingsManager::~SystemSettingsManager()
{
}

void SystemSettingsManager::requestSystemSettings(std::function<void()> callback)
{
    d->requestSystemSettings(callback);
}

SystemSettings* SystemSettingsManager::systemSettings()
{
    return d->systemSettings.get();
}

void SystemSettingsManager::saveSystemSettings(std::function<void(bool)> callback,
    nx::utils::AsyncHandlerExecutor executor,
    const common::SessionTokenHelperPtr& helper)
{
    const auto api = connectedServerApi();
    const auto tokenHelper = helper ? helper : systemContext()->getSessionTokenHelper();

    if (!NX_ASSERT(api && tokenHelper))
    {
        if (callback)
            callback(false);

        return;
    }

    auto handler =
        [callback](bool success,
            rest::Handle /*requestId*/,
            rest::ServerConnection::ErrorOrEmpty /*result*/)
        {
            if (callback)
                callback(success);

            return;
        };

    api->patchRest(tokenHelper,
        QString("/rest/v3/system/settings"),
        network::rest::Params{},
        nx::reflect::json::serialize(*d->systemSettings.get()),
        handler,
        executor);
}

} // namespace nx::vms::client::core
