#include "initial_data_loader.h"

#include <chrono>
#include <thread>

#include <nx/utils/log/log.h>

#include <nx/vms/event/event_fwd.h>

#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx {
namespace vms {
namespace utils {

static const std::chrono::milliseconds kAppServerRequestErrorTimeout(5500);

struct InitialDataLoaderFunctionsTag{};

void loadResourcesFromEcs(
    QnCommonModule* commonModule,
    ec2::AbstractECConnectionPtr ec2Connection,
    QnCommonMessageProcessor* messageProcessor,
    QnMediaServerResourcePtr mediaServer,
    std::function<bool()> needToStop)
{
    ec2::ErrorCode rez;
    {
        // read servers list
        api::MediaServerDataList mediaServerList;
        while (ec2Connection->getMediaServerManager(Qn::kSystemAccess)
            ->getServersSync(&mediaServerList) != ec2::ErrorCode::ok)
        {
            NX_ERROR(typeid(InitialDataLoaderFunctionsTag), lit("Can't get servers."));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        api::DiscoveryDataList discoveryDataList;
        while (ec2Connection->getDiscoveryManager(Qn::kSystemAccess)
            ->getDiscoveryDataSync(&discoveryDataList) != ec2::ErrorCode::ok)
        {
            NX_ERROR(typeid(InitialDataLoaderFunctionsTag), lit("Can't get discovery data."));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        QMultiHash<QnUuid, nx::utils::Url> additionalAddressesById;
        QMultiHash<QnUuid, nx::utils::Url> ignoredAddressesById;
        for (const api::DiscoveryData &data : discoveryDataList)
        {
            additionalAddressesById.insert(data.id, data.url);
            if (data.ignore)
                ignoredAddressesById.insert(data.id, data.url);
        }

        for (const auto &mediaServer : mediaServerList)
        {
            const auto defaultPort = nx::utils::Url(mediaServer.url).port();
            QList<nx::network::SocketAddress> addresses;
            ec2::deserializeNetAddrList(mediaServer.networkAddresses, addresses, defaultPort);

            QList<nx::utils::Url> additionalAddresses =
                additionalAddressesById.values(mediaServer.id);
            for (auto it = additionalAddresses.begin(); it != additionalAddresses.end();) //< no inc
            {
                const nx::network::SocketAddress addr(it->host(), it->port(defaultPort));
                if (addresses.contains(addr))
                    it = additionalAddresses.erase(it);
                else
                    ++it;
            }
            const auto dictionary = commonModule->serverAdditionalAddressesDictionary();
            dictionary->setAdditionalUrls(mediaServer.id, additionalAddresses);
            dictionary->setIgnoredUrls(mediaServer.id, ignoredAddressesById.values(mediaServer.id));
            messageProcessor->updateResource(mediaServer, ec2::NotificationSource::Local);
        }

        if (mediaServer)
        {
            do
            {
                if (needToStop())
                    return;
            } while (ec2Connection->getResourceManager(Qn::kSystemAccess)
                ->setResourceStatusSync(mediaServer->getId(), Qn::Online) != ec2::ErrorCode::ok);
        }

        // read resource status
        nx::vms::api::ResourceStatusDataList statusList;
        while ((rez = ec2Connection->getResourceManager(Qn::kSystemAccess)->getStatusListSync(
            QnUuid(),
            &statusList)) != ec2::ErrorCode::ok)
        {
            NX_DEBUG(typeid(InitialDataLoaderFunctionsTag),
                lm("Can't get properties dictionary. Reason: %1"),
                ec2::toString(rez));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetStatusList(statusList);

        // read attributes for all servers
        api::MediaServerUserAttributesDataList mediaServerUserAttributesList;
        while ((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)
            ->getUserAttributesSync(QnUuid(), &mediaServerUserAttributesList)) != ec2::ErrorCode::ok)
        {
            NX_DEBUG(typeid(InitialDataLoaderFunctionsTag),
                "Can't get server user attributes list. Reason: %1", ec2::toString(rez));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetServerUserAttributesList(mediaServerUserAttributesList);
    }

    {
        // read camera list
        nx::vms::api::CameraDataList cameras;
        while ((rez = ec2Connection->getCameraManager(Qn::kSystemAccess)
            ->getCamerasSync(&cameras)) != ec2::ErrorCode::ok)
        {
            NX_DEBUG(typeid(InitialDataLoaderFunctionsTag),
                "Can't get cameras. Reason: %1", ec2::toString(rez));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        // read camera attributes
        nx::vms::api::CameraAttributesDataList cameraUserAttributesList;
        while ((rez = ec2Connection->getCameraManager(Qn::kSystemAccess)
            ->getUserAttributesSync(&cameraUserAttributesList)) != ec2::ErrorCode::ok)
        {
            NX_DEBUG(typeid(InitialDataLoaderFunctionsTag),
                "Can't get camera user attributes list. Reason: %1", ec2::toString(rez));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetCameraUserAttributesList(cameraUserAttributesList);

        // read properties dictionary
        nx::vms::api::ResourceParamWithRefDataList kvPairs;
        while ((rez = ec2Connection->getResourceManager(Qn::kSystemAccess)
            ->getKvPairsSync(QnUuid(), &kvPairs)) != ec2::ErrorCode::ok)
        {
            NX_DEBUG(typeid(InitialDataLoaderFunctionsTag),
                "Can't get properties dictionary. Reason: %1", ec2::toString(rez));
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetPropertyList(kvPairs);

        if (commonModule->resourceDiscoveryManager())
        {
            // Properties and attributes must be read before processing cameras
            // because of getAuth() method
            std::vector<QnManualCameraInfo> manualCameras;
            for (const auto& camera: cameras)
            {
                messageProcessor->updateResource(camera, ec2::NotificationSource::Local);
                if (camera.manuallyAdded)
                {
                    QnResourceTypePtr resType = qnResTypePool->getResourceType(camera.typeId);
                    if (resType)
                    {
                        const auto auth = QnNetworkResource::getResourceAuth(commonModule,
                            camera.id, camera.typeId);
                        manualCameras.emplace_back(nx::utils::Url(camera.url), auth,
                            resType->getName(), camera.physicalId);
                    }
                    else
                    {
                        NX_ASSERT(false, lm("No resource type in the pool %1").arg(camera.typeId));
                    }
                }
            }
            commonModule->resourceDiscoveryManager()->registerManualCameras(manualCameras);
        }
    }

    {
        nx::vms::api::ServerFootageDataList serverFootageData;
        while ((rez = ec2Connection->getCameraManager(Qn::kSystemAccess)
            ->getServerFootageDataSync(&serverFootageData)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get cameras history. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        commonModule->cameraHistoryPool()->resetServerFootageData(serverFootageData);
        commonModule->cameraHistoryPool()->setHistoryCheckDelay(1000);
    }

    {
        // Loading users.
        nx::vms::api::UserDataList users;
        while ((rez = ec2Connection->getUserManager(Qn::kSystemAccess)
            ->getUsersSync(&users)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get users. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const auto &user : users)
            messageProcessor->updateResource(user, ec2::NotificationSource::Local);
    }

    {
        //loading videowalls
        nx::vms::api::VideowallDataList videowalls;
        while ((rez = ec2Connection->getVideowallManager(Qn::kSystemAccess)
            ->getVideowallsSync(&videowalls)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get videowalls. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const auto& videowall: videowalls)
            messageProcessor->updateResource(videowall, ec2::NotificationSource::Local);
    }

    {
        //loading layouts
        nx::vms::api::LayoutDataList layouts;
        while ((rez = ec2Connection->getLayoutManager(Qn::kSystemAccess)
            ->getLayoutsSync(&layouts)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get layouts. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const auto &layout : layouts)
            messageProcessor->updateResource(layout, ec2::NotificationSource::Local);
    }

    {
        //loading webpages
        nx::vms::api::WebPageDataList webpages;
        while ((rez = ec2Connection->getWebPageManager(Qn::kSystemAccess)
            ->getWebPagesSync(&webpages)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get webpages. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const auto &webpage : webpages)
            messageProcessor->updateResource(webpage, ec2::NotificationSource::Local);
    }

    {
        //loading accessible resources
        nx::vms::api::AccessRightsDataList accessRights;
        while ((rez = ec2Connection->getUserManager(Qn::kSystemAccess)
            ->getAccessRightsSync(&accessRights)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get accessRights. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetAccessRights(accessRights);
    }

    {
        // Loading user roles.
        api::UserRoleDataList userRoles;
        while ((rez = ec2Connection->getUserManager(Qn::kSystemAccess)
            ->getUserRolesSync(&userRoles)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get roles. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetUserRoles(userRoles);
    }

    {
        //Loading event rules.
        nx::vms::api::EventRuleDataList rules;
        while ((rez = ec2Connection->getEventRulesManager(Qn::kSystemAccess)
            ->getEventRulesSync(&rules)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get event rules. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }
        messageProcessor->resetEventRules(rules);
    }

    {
        // load licenses
        QnLicenseList licenses;
        while ((rez = ec2Connection->getLicenseManager(Qn::kSystemAccess)
            ->getLicensesSync(&licenses)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get license list. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const QnLicensePtr &license : licenses)
            messageProcessor->on_licenseChanged(license);
    }

    {
        // Load analytics plugins and engines.
        nx::vms::api::AnalyticsPluginDataList pluginList;
        auto analyticsManager = ec2Connection->getAnalyticsManager(Qn::kSystemAccess);

        while ((rez = analyticsManager->getAnalyticsPluginsSync(&pluginList)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get analytics plugin resource list. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const auto &plugin: pluginList)
            messageProcessor->updateResource(plugin, ec2::NotificationSource::Local);

        nx::vms::api::AnalyticsEngineDataList engineList;
        while ((rez = analyticsManager->getAnalyticsEnginesSync(&engineList)) != ec2::ErrorCode::ok)
        {
            qDebug() << "Can't get analytics engine resource list. Reason: " << ec2::toString(rez);
            std::this_thread::sleep_for(kAppServerRequestErrorTimeout);
            if (needToStop())
                return;
        }

        for (const auto &engine: engineList)
            messageProcessor->updateResource(engine, ec2::NotificationSource::Local);

    }

}

} // namespace utils
} // namespace vms
} // namespace nx
