// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_layouts_manager.h"

#include <QtCore/QTimer>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/network_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delayed.h>

#include "cross_system_layout_data.h"
#include "cross_system_layout_resource.h"

using namespace std::chrono;
using namespace nx::network::http;

namespace nx::vms::client::desktop {

namespace {

constexpr auto kRequestInterval = 30s;
constexpr auto kLayoutsPath = "/docdb/v1/docs/layouts";
const auto kParticularLayoutPathTemplate = kLayoutsPath + QString("/%1.json");

nx::utils::Url actualLayoutsEndpoint()
{
    const QString cloudLayoutsEndpointOverride(ini().cloudLayoutsEndpointOverride);
    if (!cloudLayoutsEndpointOverride.isEmpty())
    {
        const auto endpoint = nx::utils::Url::fromUserInput(cloudLayoutsEndpointOverride);
        if (NX_ASSERT(endpoint.isValid()))
            return endpoint;
    }

    return nx::network::url::Builder()
        .setScheme(kSecureUrlSchemeName)
        .setHost(nx::network::SocketGlobals::cloud().cloudHost())
        .toUrl();
}

} // namespace

using Request = std::unique_ptr<AsyncClient>;
using core::NetworkManager;

struct CloudLayoutsManager::Private
{
    CloudLayoutsManager* const q;
    QPointer<core::CloudStatusWatcher> cloudStatusWatcher = appContext()->cloudStatusWatcher();
    bool online = false;
    std::unique_ptr<SystemContext> systemContext = std::make_unique<SystemContext>(
        SystemContext::Mode::cloudLayouts,
        appContext()->peerId(),
        nx::core::access::Mode::direct);
    const nx::utils::Url endpoint = actualLayoutsEndpoint();
    std::unique_ptr<NetworkManager> networkManager = std::make_unique<NetworkManager>();
    std::unique_ptr<QTimer> timer = std::make_unique<QTimer>();
    QnUserResourcePtr user;

    Private(CloudLayoutsManager* owner):
        q(owner)
    {
        appContext()->addSystemContext(systemContext.get());
        timer->setInterval(kRequestInterval);
        timer->callOnTimeout([this](){ updateLayouts(); });
    }

    ~Private()
    {
        appContext()->removeSystemContext(systemContext.get());
    }

    Request makeRequest()
    {
        Request request = std::make_unique<AsyncClient>(
            nx::network::ssl::kDefaultCertificateCheck);
        NetworkManager::setDefaultTimeouts(request.get());

        if (NX_ASSERT(cloudStatusWatcher))
            request->setCredentials(cloudStatusWatcher->credentials());

        return request;
    }

    void addLayoutsToResourcePool(std::vector<CrossSystemLayoutData> layouts)
    {
        auto resourcePool = systemContext->resourcePool();
        const auto layoutsList = resourcePool->getResources().filtered<CrossSystemLayoutResource>(
            [](const CrossSystemLayoutResourcePtr& layout)
            {
                // Do not remove layouts which are still not saved.
                return layout->hasFlags(Qn::remote);
            });

        auto layoutsToRemove = QSet(layoutsList.begin(), layoutsList.end());

        QnResourceList newlyCreatedLayouts;
        for (const auto& layoutData: layouts)
        {
            if (layoutData.id.isNull())
                continue;

            if (auto existingLayout = resourcePool->getResourceById<CrossSystemLayoutResource>(
                layoutData.id))
            {
                layoutsToRemove.remove(existingLayout);
                if (!systemContext->layoutSnapshotManager()->isChanged(existingLayout))
                {
                    existingLayout->update(layoutData);
                    systemContext->layoutSnapshotManager()->store(existingLayout);
                }
            }
            else
            {
                auto layout = CrossSystemLayoutResourcePtr(
                    new CrossSystemLayoutResource());
                layout->setIdUnsafe(layoutData.id);
                layout->addFlags(Qn::remote);
                layout->update(layoutData);
                newlyCreatedLayouts.push_back(layout);
            }
        }

        if (!newlyCreatedLayouts.empty())
            resourcePool->addResources(newlyCreatedLayouts);

        if (!layoutsToRemove.empty())
            resourcePool->removeResources(layoutsToRemove.values());
    }

    void ensureUser()
    {
        NX_ASSERT(!user && cloudStatusWatcher);

        user = QnUserResourcePtr(new QnUserResource(api::UserType::cloud, /*externalId*/ {}));
        user->setIdUnsafe(QnUuid::createUuid());
        user->setName(cloudStatusWatcher->cloudLogin());
        user->setGroupIds({api::kAdministratorsGroupId}); //< Avoid resources access calculation.

        auto resourcePool = systemContext->resourcePool();
        resourcePool->addResource(user);
        systemContext->accessController()->setUser(user);
    }

    void updateLayouts()
    {
        Request request = makeRequest();
        auto url = endpoint;
        url.setPath(kLayoutsPath);
        url.setQuery("matchPrefix&responseType=dataOnly");
        networkManager->doGet(
            std::move(request),
            std::move(url),
            q,
            nx::utils::guarded(q,
                [this](NetworkManager::Response response)
                {
                    if (!online)
                        return;

                    if (!StatusCode::isSuccessCode(response.statusLine.statusCode))
                    {
                        NX_WARNING(this, "Layouts request failed with code %1",
                            response.statusLine.statusCode);
                        return;
                    }

                    std::vector<CrossSystemLayoutData> updatedLayouts;
                    const bool parsed = nx::reflect::json::deserialize(response.messageBody,
                        &updatedLayouts);
                    if (NX_ASSERT(parsed))
                        addLayoutsToResourcePool(std::move(updatedLayouts));
                }));
    }

    void clearContext()
    {
        auto resourcePool = systemContext->resourcePool();
        resourcePool->removeResources(resourcePool->getResources());
        user.reset();
        systemContext->accessController()->setUser({});
    }

    void sendSaveLayoutRequest(
        const CrossSystemLayoutData& layout,
        SaveCallback callback,
        bool isNewLayout)
    {
        if (!NX_ASSERT(!layout.id.isNull(), "Saving layout with null id"))
        {
            if (callback)
                executeDelayedParented([callback] { callback(/*success*/ false); }, q);
            return;
        }

        nx::network::http::Method method = isNewLayout
            ? nx::network::http::Method::post
            : nx::network::http::Method::put;

        Request request = makeRequest();
        auto url = endpoint;
        url.setPath(nx::format(kParticularLayoutPathTemplate, layout.id.toSimpleString()));
        auto messageBody = std::make_unique<BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
            nx::reflect::json::serialize(layout));
        request->setRequestBody(std::move(messageBody));
        networkManager->doRequest(
            method,
            std::move(request),
            std::move(url),
            q,
            nx::utils::guarded(q,
                [this, callback](NetworkManager::Response response)
                {
                    const bool success = StatusCode::isSuccessCode(response.statusLine.statusCode);
                    if (!success)
                    {
                        NX_WARNING(this, "Save request failed with code %1",
                            response.statusLine.statusCode);
                    }
                    if (callback)
                        callback(success);
                }));
    }

    void sendDeleteLayoutRequest(const QnUuid& id)
    {
        Request request = makeRequest();
        auto url = endpoint;
        url.setPath(nx::format(kParticularLayoutPathTemplate, id.toSimpleString()));
        networkManager->doDelete(
            std::move(request),
            std::move(url),
            q,
            nx::utils::guarded(q,
                [this](NetworkManager::Response response)
                {
                    const bool success = StatusCode::isSuccessCode(response.statusLine.statusCode);
                    if (!success)
                    {
                        NX_WARNING(this, "Delete request failed with code %1",
                            response.statusLine.statusCode);
                    }
                }));
    }

    LayoutResourcePtr convertLocalLayout(const LayoutResourcePtr& layout)
    {
        auto existingLayouts = systemContext->resourcePool()->getResources<LayoutResource>();
        QStringList usedNames;
        std::transform(
            existingLayouts.cbegin(), existingLayouts.cend(),
            std::back_inserter(usedNames), [](const auto& layout) { return layout->getName(); });

        NX_ASSERT(!layout->hasFlags(Qn::cross_system));

        auto cloudLayout = CrossSystemLayoutResourcePtr(new CrossSystemLayoutResource());
        layout->cloneTo(cloudLayout);
        cloudLayout->setIdUnsafe(QnUuid::createUuid());
        cloudLayout->setParentId(QnUuid());
        cloudLayout->addFlags(Qn::local);

        // Reset background if it is set.
        cloudLayout->setBackgroundImageFilename({});
        cloudLayout->setBackgroundOpacity(nx::vms::api::LayoutData::kDefaultBackgroundOpacity);
        cloudLayout->setBackgroundSize({});

        cloudLayout->setName(nx::utils::generateUniqueString(
            usedNames,
            tr("%1 (Copy)", "Original name will be substituted")
                .arg(cloudLayout->getName()),
            tr("%1 (Copy %2)", "Original name will be substituted as %1, counter as %2")
                .arg(cloudLayout->getName())));

        // Convert resource paths to cloud.
        auto items = cloudLayout->getItems();
        for (auto& item: items)
        {
            item.resource = descriptor(
                getResourceByDescriptor(item.resource),
                /*forceCloud*/ true);
        }
        cloudLayout->setItems(items);
        systemContext->resourcePool()->addResource(cloudLayout);

        return std::move(cloudLayout);
    }

    void saveLayout(const CrossSystemLayoutResourcePtr& layout, SaveCallback callback)
    {
        if (!NX_ASSERT(cloudStatusWatcher->status() == core::CloudStatusWatcher::Status::Online))
            return;

        auto internalCallback =
            [this, layout, callback](bool success)
            {
                if (success)
                {
                    layout->removeFlags(Qn::local);
                    layout->addFlags(Qn::remote);
                }
                if (callback)
                    callback(success);
            };

        CrossSystemLayoutData layoutData;
        fromResourceToData(layout, layoutData);

        sendSaveLayoutRequest(
            layoutData,
            internalCallback,
            /*isNewLayout*/ layout->hasFlags(Qn::local));
    }

    void deleteLayout(const QnLayoutResourcePtr& layout)
    {
        if (!NX_ASSERT(cloudStatusWatcher->status() == core::CloudStatusWatcher::Status::Online))
            return;

        sendDeleteLayoutRequest(layout->getId());
        systemContext->resourcePool()->removeResource(layout);
    }

    void setOnline(bool value)
    {
        if (value == online)
            return;

        online = value;
        if (value)
        {
            timer->start();
            ensureUser();
            updateLayouts();
        }
        else
        {
            timer->stop();
            clearContext();
        }
    }

};

CloudLayoutsManager::CloudLayoutsManager(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    NX_ASSERT(d->cloudStatusWatcher);
    connect(d->cloudStatusWatcher.data(),
        &core::CloudStatusWatcher::statusChanged,
        this,
        [this]()
        {
            d->setOnline(
                d->cloudStatusWatcher->status() == core::CloudStatusWatcher::Status::Online);
        });

     appContext()->addSystemContext(d->systemContext.get());
}

CloudLayoutsManager::~CloudLayoutsManager()
{
    d->networkManager->pleaseStopSync();
}

LayoutResourcePtr CloudLayoutsManager::convertLocalLayout(const LayoutResourcePtr& layout)
{
    return d->convertLocalLayout(layout);
}

void CloudLayoutsManager::saveLayout(
    const CrossSystemLayoutResourcePtr& layout,
    SaveCallback callback)
{
    d->saveLayout(layout, callback);
}

void CloudLayoutsManager::deleteLayout(const QnLayoutResourcePtr& layout)
{
    d->deleteLayout(layout);
}

SystemContext* CloudLayoutsManager::systemContext() const
{
    return d->systemContext.get();
}

} // namespace nx::vms::client::desktop
