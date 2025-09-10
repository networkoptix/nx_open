// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_layouts_manager.h"

#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_data.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/cloud_service_checker.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delayed.h>

#include "cross_system_layout_resource.h"

using namespace std::chrono;
using namespace nx::network::http;

namespace nx::vms::client::core {

namespace {

constexpr auto kRequestInterval = 100s;
constexpr auto kDefaultRequestTimeout = 30s;
constexpr auto kLayoutsPath = "/docdb/v1/docs/layouts";
const auto kParticularLayoutPathTemplate = kLayoutsPath + QString("/%1.json");

nx::Url actualLayoutsEndpoint()
{
    const QString cloudLayoutsEndpointOverride(ini().cloudLayoutsEndpointOverride);
    if (!cloudLayoutsEndpointOverride.isEmpty())
    {
        const auto endpoint = nx::Url::fromUserInput(cloudLayoutsEndpointOverride);
        if (NX_ASSERT(endpoint.isValid()))
            return endpoint;
    }

    // Some tests do not use network. Socket globals are unaccessible in this case.
    const QString cloudHost = nx::network::SocketGlobals::isInitialized()
        ? QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost())
        : nx::branding::cloudHost();

    return nx::network::url::Builder()
        .setScheme(kSecureUrlSchemeName)
        .setHost(cloudHost)
        .toUrl();
}

bool isWebPage(const QString& url)
{
    return nx::network::http::isUrlScheme(nx::Url(url).scheme());
}

} // namespace

struct CloudLayoutsManager::Private
{
    using CloudStatus = core::CloudStatusWatcher::Status;

    CloudLayoutsManager* const q;
    QPointer<core::CloudStatusWatcher> cloudStatusWatcher = appContext()->cloudStatusWatcher();
    CloudStatus status = CloudStatus::Offline;
    std::unique_ptr<SystemContext> systemContext{appContext()->createSystemContext(
        SystemContext::Mode::cloudLayouts)};
    nx::cloud::db::client::ApiRequestsExecutor apiRequestExecutor;
    std::unique_ptr<QTimer> timer = std::make_unique<QTimer>();
    QnUserResourcePtr user;

    Private(CloudLayoutsManager* owner):
        q(owner),
        apiRequestExecutor(actualLayoutsEndpoint(), nx::network::ssl::kDefaultCertificateCheck)
    {
        systemContext->setObjectName("Cloud Layouts System Context");
        appContext()->addSystemContext(systemContext.get());

        apiRequestExecutor.setCacheEnabled(true);
        apiRequestExecutor.setRequestTimeout(kDefaultRequestTimeout);

        timer->setInterval(kRequestInterval);
        timer->callOnTimeout([this](){ updateLayouts(); });
    }

    ~Private()
    {
        appContext()->removeSystemContext(systemContext.release());
    }

    void addLayoutsToResourcePool(const std::vector<core::CrossSystemLayoutData>& layouts)
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
                if (!existingLayout->isChanged())
                {
                    existingLayout->update(layoutData);
                    existingLayout->storeSnapshot();
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

        createResourcesFromLayout(layouts);
    }

    void createResourcesFromLayout(
        const std::vector<core::CrossSystemLayoutData>& crossSystemLayoutsList)
    {
        QnResourceList newlyCreatedResources;
        auto resourcePool = systemContext->resourcePool();
        auto resourcesToRemove = resourcePool->getResources().filtered(
            [](const QnResourcePtr& resource)
            {
                return resource->hasFlags(Qn::local_media | Qn::web_page);
            });

        for (const auto& layout: crossSystemLayoutsList)
        {
            for (const auto& item: layout.items)
            {
                QnResourcePtr resource = resourcePool->getResourceById(item.resourceId);
                if (resource)
                {
                    resource->setUrl(item.resourcePath);
                    resource->setName(item.name);
                    resourcesToRemove.removeOne(resource);
                    continue;
                }

                if (isWebPage(item.resourcePath))
                {
                    resource = QnResourcePtr(new QnWebPageResource());
                }
                else if (FileTypeSupport::isMovieFileExt(item.resourcePath)
                    || FileTypeSupport::isImageFileExt(item.resourcePath))
                {
                    resource = QnResourcePtr(new QnAviResource(
                        QDir::fromNativeSeparators(item.resourcePath)));
                }
                else
                {
                    continue;
                }

                resource->setIdUnsafe(item.resourceId);
                resource->setUrl(item.resourcePath);
                resource->setName(item.name);
                newlyCreatedResources.push_back(resource);
            }
        }

        if (!newlyCreatedResources.empty())
            resourcePool->addResources(newlyCreatedResources);

        if (!resourcesToRemove.empty())
            resourcePool->removeResources(resourcesToRemove);
    }

    void ensureUser()
    {
        if (user)
            return;

        if (!NX_ASSERT(cloudStatusWatcher))
            return;

        user = QnUserResourcePtr(new QnUserResource(api::UserType::cloud, /*externalId*/ {}));
        user->setIdUnsafe(nx::Uuid::createUuid());
        user->setName(cloudStatusWatcher->cloudLogin());
        user->setSiteGroupIds({api::kAdministratorsGroupId}); //< Avoid resources access calculation.

        auto resourcePool = systemContext->resourcePool();
        resourcePool->addResource(user);
        systemContext->accessController()->setUser(user);
    }

    void updateLayouts()
    {
        if (!appContext()->cloudServiceChecker()->hasService(
                nx::vms::client::core::CloudService::docdb))
        {
            return;
        }

        QUrlQuery query;
        query.addQueryItem("matchPrefix", "");
        query.addQueryItem("responseType", "dataOnly");

        apiRequestExecutor.makeAsyncCall<std::vector<core::CrossSystemLayoutData>>(
            network::http::Method::get,
            kLayoutsPath,
            UrlQuery(query),
            utils::guarded(
                q,
                [this](
                    cloud::db::api::ResultCode resultCode,
                    std::vector<core::CrossSystemLayoutData> layoutsData)
                {
                    if (resultCode != nx::cloud::db::api::ResultCode::ok)
                    {
                        NX_WARNING(this, "Layouts request failed with code %1", resultCode);
                        return;
                    }

                    addLayoutsToResourcePool(std::move(layoutsData));
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
        const core::CrossSystemLayoutData& layout,
        SaveCallback callback,
        bool isNewLayout)
    {
        if (!NX_ASSERT(!layout.id.isNull(), "Saving layout with null id"))
        {
            if (callback)
                executeDelayedParented([callback] { callback(/*success*/ false); }, q);
            return;
        }

        apiRequestExecutor.makeAsyncCall<void>(
            isNewLayout ? network::http::Method::post : network::http::Method::put,
            nx::format(kParticularLayoutPathTemplate, layout.id.toSimpleString()).toStdString(),
            /*urlQuery*/ {},
            layout,
            utils::guarded(q,
                [this, callback = std::move(callback)](cloud::db::api::ResultCode resultCode)
                {
                    const bool success = resultCode == cloud::db::api::ResultCode::ok
                        || resultCode == cloud::db::api::ResultCode::created;
                    if (!success)
                        NX_WARNING(this, "Save request failed with code %1", resultCode);

                    if (callback)
                        callback(success);
                }));
    }

    void sendDeleteLayoutRequest(const nx::Uuid& id)
    {
        apiRequestExecutor.makeAsyncCall<void>(
            network::http::Method::delete_,
            nx::format(kParticularLayoutPathTemplate, id.toSimpleString()).toStdString(),
            /*urlQuery*/ {},
            utils::guarded(q,
                [this](cloud::db::api::ResultCode resultCode)
                {
                    if (resultCode != cloud::db::api::ResultCode::ok)
                        NX_WARNING(this, "Delete request failed with code %1", resultCode);
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
        cloudLayout->setIdUnsafe(nx::Uuid::createUuid());
        cloudLayout->setParentId(nx::Uuid());
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

        common::LayoutItemDataMap items;
        for (auto [id, item]: cloudLayout->getItems().asKeyValueRange()) //< Copy is intended.
        {
            const auto resource = core::getResourceByDescriptor(item.resource);
            if (!resource)
                continue;

            item.resource = core::descriptor(resource, /*forceCloud*/ true);
            items[id] = item;
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
            [layout, callback = std::move(callback)](bool success)
            {
                if (success)
                {
                    layout->removeFlags(Qn::local);
                    layout->addFlags(Qn::remote);
                }
                if (callback)
                    callback(success);
            };

        core::CrossSystemLayoutData layoutData;
        core::fromResourceToData(layout, layoutData);

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

    void setCloudStatus(CloudStatus value)
    {
        if (value == status)
            return;

        const auto oldStatus = value;
        status = value;

        if (status == CloudStatus::Online)
        {
            apiRequestExecutor.httpClientOptions().setCredentials(
                cloudStatusWatcher->credentials());

            timer->start();
            ensureUser();
            updateLayouts();
        }
        else if (status == CloudStatus::LoggedOut)
        {
            timer->stop();
            clearContext();
        }
        else if (oldStatus == CloudStatus::Online && status == CloudStatus::Offline)
        {
            // There is no need to turn resource status offline. The servers and cameras may be
            // still available via local network.
            timer->stop();
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
        [this](Private::CloudStatus status)
        {
            d->setCloudStatus(status);
        });
}

CloudLayoutsManager::~CloudLayoutsManager()
{
    d->apiRequestExecutor.pleaseStopSync();
}

LayoutResourcePtr CloudLayoutsManager::convertLocalLayout(const LayoutResourcePtr& layout)
{
    return d->convertLocalLayout(layout);
}

void CloudLayoutsManager::saveLayout(
    const CrossSystemLayoutResourcePtr& layout,
    SaveCallback callback)
{
    d->saveLayout(layout, std::move(callback));
}

void CloudLayoutsManager::deleteLayout(const QnLayoutResourcePtr& layout)
{
    d->deleteLayout(layout);
}

void CloudLayoutsManager::updateLayouts()
{
    d->updateLayouts();
}

SystemContext* CloudLayoutsManager::systemContext() const
{
    return d->systemContext.get();
}

} // namespace nx::vms::client::core
