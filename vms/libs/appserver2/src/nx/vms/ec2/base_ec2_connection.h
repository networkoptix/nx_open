// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QUrlQuery>

#include <core/resource_management/resource_pool.h>
#include <managers/analytics_manager.h>
#include <managers/camera_manager.h>
#include <managers/discovery_manager.h>
#include <managers/event_rules_manager.h>
#include <managers/layout_manager.h>
#include <managers/license_manager.h>
#include <managers/lookup_list_manager.h>
#include <managers/media_server_manager.h>
#include <managers/misc_manager.h>
#include <managers/resource_manager.h>
#include <managers/showreel_manager.h>
#include <managers/stored_file_manager.h>
#include <managers/user_manager.h>
#include <managers/videowall_manager.h>
#include <managers/vms_rules_manager.h>
#include <managers/webpage_manager.h>
#include <nx/vms/time/abstract_time_sync_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <transaction/message_bus_adapter.h>

#include "ec_connection_notification_manager.h"

namespace ec2 {

// TODO: #2.4 remove Ec2 suffix to avoid ec2::BaseEc2Connection
template<class QueryProcessorType>
class BaseEc2Connection
:
    public AbstractECConnection
{
public:
    BaseEc2Connection(QueryProcessorType* queryProcessor);
    virtual ~BaseEc2Connection();

    void init(nx::vms::discovery::Manager* discoveryManager);

    virtual AbstractResourceManagerPtr getResourceManager(const Qn::UserSession& userSession) override;
    virtual AbstractMediaServerManagerPtr getMediaServerManager(const Qn::UserSession& userSession) override;
    virtual AbstractCameraManagerPtr getCameraManager(const Qn::UserSession& userSession) override;
    virtual AbstractLicenseManagerPtr getLicenseManager(const Qn::UserSession& userSession) override;
    virtual AbstractEventRulesManagerPtr getEventRulesManager(const Qn::UserSession& userSession) override;
    virtual AbstractVmsRulesManagerPtr getVmsRulesManager(const Qn::UserSession& userSession) override;
    virtual AbstractUserManagerPtr getUserManager(const Qn::UserSession& userSession) override;
    virtual AbstractLayoutManagerPtr getLayoutManager(const Qn::UserSession& userSession) override;
    virtual AbstractShowreelManagerPtr getShowreelManager(const Qn::UserSession& userSession) override;
    virtual AbstractVideowallManagerPtr getVideowallManager(const Qn::UserSession& userSession) override;
    virtual AbstractWebPageManagerPtr getWebPageManager(const Qn::UserSession& userSession) override;
    virtual AbstractStoredFileManagerPtr getStoredFileManager(const Qn::UserSession& userSession) override;
    virtual AbstractMiscManagerPtr getMiscManager(const Qn::UserSession& userSession) override;
    virtual AbstractDiscoveryManagerPtr getDiscoveryManager(const Qn::UserSession& userSession) override;
    virtual AbstractAnalyticsManagerPtr getAnalyticsManager(const Qn::UserSession& userSession) override;
    virtual AbstractLookupListManagerPtr getLookupListManager(const Qn::UserSession& userSession) override;

    virtual AbstractLicenseNotificationManagerPtr licenseNotificationManager() override;
    virtual AbstractTimeNotificationManagerPtr timeNotificationManager() override;
    virtual AbstractResourceNotificationManagerPtr resourceNotificationManager() override;
    virtual AbstractMediaServerNotificationManagerPtr mediaServerNotificationManager() override;
    virtual AbstractCameraNotificationManagerPtr cameraNotificationManager() override;
    virtual AbstractBusinessEventNotificationManagerPtr businessEventNotificationManager() override;
    virtual AbstractVmsRulesNotificationManagerPtr vmsRulesNotificationManager() override;
    virtual AbstractUserNotificationManagerPtr userNotificationManager() override;
    virtual AbstractLayoutNotificationManagerPtr layoutNotificationManager() override;
    virtual AbstractShowreelNotificationManagerPtr showreelNotificationManager() override;
    virtual AbstractWebPageNotificationManagerPtr webPageNotificationManager() override;
    virtual AbstractDiscoveryNotificationManagerPtr discoveryNotificationManager() override;
    virtual AbstractMiscNotificationManagerPtr miscNotificationManager() override;
    virtual AbstractStoredFileNotificationManagerPtr storedFileNotificationManager() override;
    virtual AbstractVideowallNotificationManagerPtr videowallNotificationManager() override;
    virtual AbstractAnalyticsNotificationManagerPtr analyticsNotificationManager() override;
    virtual AbstractLookupListNotificationManagerPtr lookupListNotificationManager() override;

    virtual void startReceivingNotifications() override;
    virtual void stopReceivingNotifications() override;

    virtual int dumpDatabase(
        Handler<nx::vms::api::DatabaseDumpData> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int dumpDatabaseToFile(
        const QString& dumpFilePath,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int restoreDatabase(
        const nx::vms::api::DatabaseDumpData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual void addRemotePeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url,
        nx::network::ssl::AdapterFunc adapterFunc) override;

    virtual void deleteRemotePeer(const QnUuid& id) override;

    QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
    virtual ECConnectionNotificationManager* notificationManager() override;

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer,
        int* distance,
        nx::network::SocketAddress* knownPeerAddress) const override;

protected:
    QueryProcessorType* m_queryProcessor;
    QnLicenseNotificationManagerPtr m_licenseNotificationManager;
    QnResourceNotificationManagerPtr m_resourceNotificationManager;
    QnMediaServerNotificationManagerPtr m_mediaServerNotificationManager;
    QnCameraNotificationManagerPtr m_cameraNotificationManager;
    QnUserNotificationManagerPtr m_userNotificationManager;
    QnBusinessEventNotificationManagerPtr m_businessEventNotificationManager;
    VmsRulesNotificationManagerPtr m_vmsRulesNotificationManager;
    QnLayoutNotificationManagerPtr m_layoutNotificationManager;
    ShowreelNotificationManagerPtr m_showreelNotificationManager;
    QnVideowallNotificationManagerPtr m_videowallNotificationManager;
    QnWebPageNotificationManagerPtr m_webPageNotificationManager;
    QnStoredFileNotificationManagerPtr m_storedFileNotificationManager;
    QnMiscNotificationManagerPtr m_miscNotificationManager;
    QnDiscoveryNotificationManagerPtr m_discoveryNotificationManager;
    QnTimeNotificationManagerPtr m_timeNotificationManager;
    AnalyticsNotificationManagerPtr m_analyticsNotificationManager;
    LookupListNotificationManagerPtr m_lookupListNotificationManager;
    std::unique_ptr<ECConnectionNotificationManager> m_notificationManager;
};

template<class QueryProcessorType>
BaseEc2Connection<QueryProcessorType>::BaseEc2Connection(QueryProcessorType* queryProcessor):
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
BaseEc2Connection<QueryProcessorType>::~BaseEc2Connection()
{
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::init(nx::vms::discovery::Manager* discoveryManager)
{
    m_licenseNotificationManager = std::make_shared<QnLicenseNotificationManager>();
    m_resourceNotificationManager = std::make_shared<QnResourceNotificationManager>();
    m_mediaServerNotificationManager = std::make_shared<QnMediaServerNotificationManager>();
    m_cameraNotificationManager = std::make_shared<QnCameraNotificationManager>();
    m_userNotificationManager = std::make_shared<QnUserNotificationManager>();
    m_businessEventNotificationManager = std::make_shared<QnBusinessEventNotificationManager>();
    m_vmsRulesNotificationManager = std::make_shared<VmsRulesNotificationManager>();
    m_layoutNotificationManager = std::make_shared<QnLayoutNotificationManager>();
    m_showreelNotificationManager = std::make_shared<ShowreelNotificationManager>();
    m_videowallNotificationManager = std::make_shared<QnVideowallNotificationManager>();
    m_webPageNotificationManager = std::make_shared<QnWebPageNotificationManager>();
    m_storedFileNotificationManager = std::make_shared<QnStoredFileNotificationManager>();
    m_miscNotificationManager = std::make_shared<QnMiscNotificationManager>();
    m_discoveryNotificationManager = std::make_shared<QnDiscoveryNotificationManager>(
        discoveryManager);
    m_timeNotificationManager = std::make_shared<QnTimeNotificationManager>();
    m_analyticsNotificationManager = std::make_shared<AnalyticsNotificationManager>();
    m_lookupListNotificationManager = std::make_shared<LookupListNotificationManager>();
    m_notificationManager = std::make_unique<ECConnectionNotificationManager>(
        this,
        m_licenseNotificationManager.get(),
        m_resourceNotificationManager.get(),
        m_mediaServerNotificationManager.get(),
        m_cameraNotificationManager.get(),
        m_userNotificationManager.get(),
        m_timeNotificationManager.get(),
        m_businessEventNotificationManager.get(),
        m_vmsRulesNotificationManager.get(),
        m_layoutNotificationManager.get(),
        m_showreelNotificationManager.get(),
        m_videowallNotificationManager.get(),
        m_webPageNotificationManager.get(),
        m_storedFileNotificationManager.get(),
        m_miscNotificationManager.get(),
        m_discoveryNotificationManager.get(),
        m_analyticsNotificationManager.get(),
        m_lookupListNotificationManager.get());
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::startReceivingNotifications()
{
    connect(messageBus(), &AbstractTransactionMessageBus::peerFound,
        this, &BaseEc2Connection<QueryProcessorType>::remotePeerFound, Qt::DirectConnection);
    connect(messageBus(), &AbstractTransactionMessageBus::peerLost,
        this, &BaseEc2Connection<QueryProcessorType>::remotePeerLost, Qt::DirectConnection);
    connect(messageBus(), &AbstractTransactionMessageBus::remotePeerUnauthorized,
        this, &BaseEc2Connection<QueryProcessorType>::remotePeerUnauthorized, Qt::DirectConnection);
    connect(messageBus(), &AbstractTransactionMessageBus::newDirectConnectionEstablished,
        this, &BaseEc2Connection<QueryProcessorType>::newDirectConnectionEstablished, Qt::DirectConnection);

    messageBus()->start();
    timeSyncManager()->start();
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::stopReceivingNotifications()
{
    timeSyncManager()->stop();
    messageBus()->disconnectAndJoin(this);
    messageBus()->stop();
}

template<class QueryProcessorType>
AbstractResourceManagerPtr BaseEc2Connection<QueryProcessorType>::getResourceManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnResourceManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractResourceNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::resourceNotificationManager()
{
    return m_resourceNotificationManager;
}

template<class QueryProcessorType>
AbstractMediaServerManagerPtr BaseEc2Connection<QueryProcessorType>::getMediaServerManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnMediaServerManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractMediaServerNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::mediaServerNotificationManager()
{
    return m_mediaServerNotificationManager;
}

template<class QueryProcessorType>
AbstractCameraManagerPtr BaseEc2Connection<QueryProcessorType>::getCameraManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnCameraManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractCameraNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::cameraNotificationManager()
{
    return m_cameraNotificationManager;
}

template<class QueryProcessorType>
AbstractLicenseManagerPtr BaseEc2Connection<QueryProcessorType>::getLicenseManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnLicenseManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractLicenseNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::licenseNotificationManager()
{
    return m_licenseNotificationManager;
}

template<class QueryProcessorType>
AbstractEventRulesManagerPtr BaseEc2Connection<QueryProcessorType>::getEventRulesManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<EventRulesManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractVmsRulesManagerPtr BaseEc2Connection<QueryProcessorType>::getVmsRulesManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<VmsRulesManager<QueryProcessorType>>(m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractBusinessEventNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::businessEventNotificationManager()
{
    return m_businessEventNotificationManager;
}

template<class QueryProcessorType>
AbstractVmsRulesNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::vmsRulesNotificationManager()
{
    return m_vmsRulesNotificationManager;
}

template<class QueryProcessorType>
AbstractUserManagerPtr BaseEc2Connection<QueryProcessorType>::getUserManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnUserManager<QueryProcessorType>>(m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractUserNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::userNotificationManager()
{
    return m_userNotificationManager;
}

template<class QueryProcessorType>
AbstractLayoutManagerPtr BaseEc2Connection<QueryProcessorType>::getLayoutManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnLayoutManager<QueryProcessorType>>(m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractShowreelManagerPtr BaseEc2Connection<QueryProcessorType>::getShowreelManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<ShowreelManager<QueryProcessorType>>(m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractLayoutNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::layoutNotificationManager()
{
    return m_layoutNotificationManager;
}

template<class QueryProcessorType>
AbstractShowreelNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::showreelNotificationManager()
{
    return m_showreelNotificationManager;
}

template<class QueryProcessorType>
AbstractVideowallManagerPtr BaseEc2Connection<QueryProcessorType>::getVideowallManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnVideowallManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<typename QueryProcessorType>
AbstractVideowallNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::videowallNotificationManager()
{
    return m_videowallNotificationManager;
}

template<class QueryProcessorType>
AbstractWebPageManagerPtr BaseEc2Connection<QueryProcessorType>::getWebPageManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnWebPageManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractWebPageNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::webPageNotificationManager()
{
    return m_webPageNotificationManager;
}

template<class QueryProcessorType>
AbstractStoredFileManagerPtr BaseEc2Connection<QueryProcessorType>::getStoredFileManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnStoredFileManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<typename QueryProcessorType>
AbstractStoredFileNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::storedFileNotificationManager()
{
    return m_storedFileNotificationManager;
}

template<class QueryProcessorType>
AbstractMiscManagerPtr BaseEc2Connection<QueryProcessorType>::getMiscManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnMiscManager<QueryProcessorType>>(m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractMiscNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::miscNotificationManager()
{
    return m_miscNotificationManager;
}

template<class QueryProcessorType>
AbstractDiscoveryManagerPtr BaseEc2Connection<QueryProcessorType>::getDiscoveryManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<QnDiscoveryManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractDiscoveryNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::discoveryNotificationManager()
{
    return m_discoveryNotificationManager;
}

template<class QueryProcessorType>
AbstractTimeNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::timeNotificationManager()
{
    return m_timeNotificationManager;
}

template<class QueryProcessorType>
AbstractAnalyticsManagerPtr BaseEc2Connection<QueryProcessorType>::getAnalyticsManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<AnalyticsManager<QueryProcessorType>>(
        m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractLookupListManagerPtr BaseEc2Connection<QueryProcessorType>::getLookupListManager(
    const Qn::UserSession& userSession)
{
    return std::make_shared<LookupListManager<QueryProcessorType>>(m_queryProcessor, userSession);
}

template<class QueryProcessorType>
AbstractLookupListNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::lookupListNotificationManager()
{
    return m_lookupListNotificationManager;
}

template<class QueryProcessorType>
AbstractAnalyticsNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::analyticsNotificationManager()
{
    return m_analyticsNotificationManager;
}

template<class QueryProcessorType>
ECConnectionNotificationManager* BaseEc2Connection<QueryProcessorType>::notificationManager()
{
    return m_notificationManager.get();
}

template<class QueryProcessorType>
int BaseEc2Connection<QueryProcessorType>::dumpDatabase(
    Handler<nx::vms::api::DatabaseDumpData> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(Qn::kSystemAccess)
        .template processQueryAsync<std::nullptr_t, nx::vms::api::DatabaseDumpData>(
            ApiCommand::dumpDatabase,
            nullptr,
            [requestId, handler = handlerExecutor.bind(std::move(handler))](
                auto&&... args) mutable
            {
                handler(requestId, std::move(args)...);
            });
    return requestId;
}

template<class QueryProcessorType>
int BaseEc2Connection<QueryProcessorType>::dumpDatabaseToFile(
    const QString& dumpFilePath,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::StoredFilePath dumpFilePathData;
    dumpFilePathData.path = dumpFilePath;

    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(Qn::kSystemAccess)
        .template processQueryAsync<
            nx::vms::api::StoredFilePath, nx::vms::api::DatabaseDumpToFileData>(
                ApiCommand::dumpDatabaseToFile,
                dumpFilePathData,
                [requestId, handler = handlerExecutor.bind(std::move(handler))](
                    Result result, auto&&...) mutable
                {
                    handler(requestId, std::move(result));
                });
    return requestId;
}

template<class QueryProcessorType>
int BaseEc2Connection<QueryProcessorType>::restoreDatabase(
    const nx::vms::api::DatabaseDumpData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(Qn::kSystemAccess).processUpdateAsync(
        ApiCommand::restoreDatabase,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::addRemotePeer(
    const QnUuid& id,
    nx::vms::api::PeerType peerType,
    const nx::utils::Url &_url,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    nx::utils::Url url(_url);
    QUrlQuery q;
    url.setQuery(q);
    messageBus()->addOutgoingConnectionToPeer(
        id, peerType, url, /*credentials*/ std::nullopt, std::move(adapterFunc));
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::deleteRemotePeer(const QnUuid& id)
{
    messageBus()->removeOutgoingConnectionFromPeer(id);
}

template<class QueryProcessorType>
QnUuid BaseEc2Connection<QueryProcessorType>::routeToPeerVia(
    const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const
{
    auto messageBus = this->messageBus();
    return messageBus ? messageBus->routeToPeerVia(dstPeer, distance, knownPeerAddress) : QnUuid();
}

} // namespace ec2
