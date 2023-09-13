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

    bool loadSerializedSettings(const QByteArray& data)
    {
        nx::reflect::DeserializationResult deserializationResult;
        SystemSettings result;

        std::tie(result, deserializationResult) =
            nx::reflect::json::deserialize<SystemSettings>(data.data());
        if (!NX_ASSERT(deserializationResult.success, "Settings cannot be deserialized"))
            return false;

        NX_VERBOSE(this, "System settings received");

        auto hasChange = *systemSettings != result;
        if (hasChange)
            *systemSettings = result;

        if (hasChange)
            emit q->systemSettingsChanged();

        return true;
    }

    rest::Handle requestSystemSettings(RequestCallback callback = {})
    {
        const auto internalCallback = nx::utils::guarded(q,
            [this, callback=std::move(callback)](
                bool success,
                rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders)
            {
                if (success)
                    success = loadSerializedSettings(data);
                else
                    NX_WARNING(this, "System settings request failed");

                if (callback)
                    callback(success, requestId);
            }
        );

        auto api = q->connectedServerApi();
        if (!NX_ASSERT(api))
            return rest::Handle{};

        return api->getRawResult(
            "/rest/v3/system/settings",
            {},
            internalCallback,
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

rest::Handle SystemSettingsManager::requestSystemSettings(RequestCallback callback)
{
    return d->requestSystemSettings(std::move(callback));
}

SystemSettings* SystemSettingsManager::systemSettings()
{
    return d->systemSettings.get();
}

rest::Handle SystemSettingsManager::saveSystemSettings(
    RequestCallback callback,
    nx::utils::AsyncHandlerExecutor executor,
    const common::SessionTokenHelperPtr& helper)
{
    const auto api = connectedServerApi();
    const auto tokenHelper = helper ? helper : systemContext()->getSessionTokenHelper();

    if (!NX_ASSERT(api && tokenHelper))
        return rest::Handle{};

    auto handler = nx::utils::guarded(this,
        [this, callback = std::move(callback)](
            bool success,
            rest::Handle requestId,
            rest::ErrorOrData<QByteArray> result)
        {
            if (auto data = std::get_if<QByteArray>(&result))
            {
                d->loadSerializedSettings(*data);
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                NX_WARNING(this, "Error while applying changes: %1", error->errorString);
                success = false;
            }

            if (callback)
                callback(success, requestId);
        });

    return api->patchRest(tokenHelper,
        QString("/rest/v3/system/settings"),
        network::rest::Params{},
        nx::reflect::json::serialize(*d->systemSettings.get()),
        handler,
        executor);
}

} // namespace nx::vms::client::core
