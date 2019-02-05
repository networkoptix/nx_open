#pragma once

#include <memory>

#include <core/resource_management/resource_pool.h>
#include <ec_connection_notification_manager.h>
#include <ec_connection_audit_manager.h>
#include <managers/event_rules_manager.h>
#include <managers/camera_manager.h>
#include <managers/layout_manager.h>
#include <managers/layout_tour_manager.h>
#include <managers/license_manager.h>
#include <managers/stored_file_manager.h>
#include <managers/media_server_manager.h>
#include <managers/resource_manager.h>
#include <managers/user_manager.h>
#include <managers/videowall_manager.h>
#include <managers/webpage_manager.h>
#include <managers/updates_manager.h>
#include <managers/misc_manager.h>
#include <managers/discovery_manager.h>
#include <managers/analytics_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <transaction/message_bus_adapter.h>

#include <nx/vms/time_sync/time_sync_manager.h>

namespace ec2 {

class ECConnectionNotificationManager;
class Ec2DirectConnectionFactory;

// TODO: #2.4 remove Ec2 suffix to avoid ec2::BaseEc2Connection
template<class QueryProcessorType>
class BaseEc2Connection
:
    public AbstractECConnection
{
public:
    BaseEc2Connection(
        const AbstractECConnectionFactory* connectionFactory,
        QueryProcessorType* queryProcessor);
    virtual ~BaseEc2Connection();

    virtual AbstractResourceManagerPtr makeResourceManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractMediaServerManagerPtr makeMediaServerManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractCameraManagerPtr makeCameraManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractLicenseManagerPtr makeLicenseManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractEventRulesManagerPtr makeEventRulesManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractUserManagerPtr makeUserManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractLayoutManagerPtr makeLayoutManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractLayoutTourManagerPtr makeLayoutTourManager(const Qn::UserAccessData& userAccessData) override;
    virtual AbstractVideowallManagerPtr makeVideowallManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractWebPageManagerPtr makeWebPageManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractStoredFileManagerPtr makeStoredFileManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractUpdatesManagerPtr makeUpdatesManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractMiscManagerPtr makeMiscManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractDiscoveryManagerPtr makeDiscoveryManager(const Qn::UserAccessData &userAccessData) override;
    virtual AbstractAnalyticsManagerPtr makeAnalyticsManager(
        const Qn::UserAccessData &userAccessData) override;

    virtual AbstractLicenseNotificationManagerPtr licenseNotificationManager() override;
    virtual AbstractTimeNotificationManagerPtr timeNotificationManager() override;
    virtual AbstractResourceNotificationManagerPtr resourceNotificationManager() override;
    virtual AbstractMediaServerNotificationManagerPtr mediaServerNotificationManager() override;
    virtual AbstractCameraNotificationManagerPtr cameraNotificationManager() override;
    virtual AbstractBusinessEventNotificationManagerPtr businessEventNotificationManager() override;
    virtual AbstractUserNotificationManagerPtr userNotificationManager() override;
    virtual AbstractLayoutNotificationManagerPtr layoutNotificationManager() override;
    virtual AbstractLayoutTourNotificationManagerPtr layoutTourNotificationManager() override;
    virtual AbstractWebPageNotificationManagerPtr webPageNotificationManager() override;
    virtual AbstractDiscoveryNotificationManagerPtr discoveryNotificationManager() override;
    virtual AbstractMiscNotificationManagerPtr miscNotificationManager() override;
    virtual AbstractUpdatesNotificationManagerPtr updatesNotificationManager() override;
    virtual AbstractStoredFileNotificationManagerPtr storedFileNotificationManager() override;
    virtual AbstractVideowallNotificationManagerPtr videowallNotificationManager() override;
    virtual AbstractAnalyticsNotificationManagerPtr
        analyticsNotificationManager() override;

    virtual QnCommonModule* commonModule() const override;

    virtual void startReceivingNotifications() override;
    virtual void stopReceivingNotifications() override;

    virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
    virtual int dumpDatabaseToFileAsync(
        const QString& dumpFilePath,
        impl::SimpleHandlerPtr) override;
    virtual int restoreDatabaseAsync(
        const nx::vms::api::DatabaseDumpData& data,
        impl::SimpleHandlerPtr handler) override;

    virtual void addRemotePeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url) override;
    virtual void deleteRemotePeer(const QnUuid& id) override;

    QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
    virtual ECConnectionNotificationManager* notificationManager() override;
    ECConnectionAuditManager* auditManager() { return m_auditManager.get(); }

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer,
        int* distance,
        nx::network::SocketAddress* knownPeerAddress) const override;

    virtual TransactionMessageBusAdapter* messageBus() const override;
    virtual nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager() const override;
protected:
    const AbstractECConnectionFactory* m_connectionFactory;
    QueryProcessorType* m_queryProcessor;
    QnLicenseNotificationManagerPtr m_licenseNotificationManager;
    QnResourceNotificationManagerPtr m_resourceNotificationManager;
    QnMediaServerNotificationManagerPtr m_mediaServerNotificationManager;
    QnCameraNotificationManagerPtr m_cameraNotificationManager;
    QnUserNotificationManagerPtr m_userNotificationManager;
    QnBusinessEventNotificationManagerPtr m_businessEventNotificationManager;
    QnLayoutNotificationManagerPtr m_layoutNotificationManager;
    QnLayoutTourNotificationManagerPtr m_layoutTourNotificationManager;
    QnVideowallNotificationManagerPtr m_videowallNotificationManager;
    QnWebPageNotificationManagerPtr m_webPageNotificationManager;
    QnStoredFileNotificationManagerPtr m_storedFileNotificationManager;
    QnUpdatesNotificationManagerPtr m_updatesNotificationManager;
    QnMiscNotificationManagerPtr m_miscNotificationManager;
    QnDiscoveryNotificationManagerPtr m_discoveryNotificationManager;
    QnTimeNotificationManagerPtr m_timeNotificationManager;
    AnalyticsNotificationManagerPtr m_analyticsNotificationManager;
    std::unique_ptr<ECConnectionNotificationManager> m_notificationManager;
    std::unique_ptr<ECConnectionAuditManager> m_auditManager;
};

template<class QueryProcessorType>
BaseEc2Connection<QueryProcessorType>::BaseEc2Connection(
    const AbstractECConnectionFactory* connectionFactory,
    QueryProcessorType* queryProcessor)
    :
    m_connectionFactory(connectionFactory),
    m_queryProcessor(queryProcessor),
    m_licenseNotificationManager(new QnLicenseNotificationManager),
    m_resourceNotificationManager(new QnResourceNotificationManager),
    m_mediaServerNotificationManager(new QnMediaServerNotificationManager),
    m_cameraNotificationManager(new QnCameraNotificationManager),
    m_userNotificationManager(new QnUserNotificationManager),
    m_businessEventNotificationManager(new QnBusinessEventNotificationManager),
    m_layoutNotificationManager(new QnLayoutNotificationManager),
    m_layoutTourNotificationManager(new QnLayoutTourNotificationManager),
    m_videowallNotificationManager(new QnVideowallNotificationManager),
    m_webPageNotificationManager(new QnWebPageNotificationManager),
    m_storedFileNotificationManager(new QnStoredFileNotificationManager),
    m_updatesNotificationManager(new QnUpdatesNotificationManager),
    m_miscNotificationManager(new QnMiscNotificationManager),
    m_discoveryNotificationManager(new QnDiscoveryNotificationManager(commonModule())),
    m_timeNotificationManager(new QnTimeNotificationManager(connectionFactory->timeSyncManager())),
    m_analyticsNotificationManager(new AnalyticsNotificationManager())
{
    m_notificationManager.reset(
        new ECConnectionNotificationManager(
            this,
            m_licenseNotificationManager.get(),
            m_resourceNotificationManager.get(),
            m_mediaServerNotificationManager.get(),
            m_cameraNotificationManager.get(),
            m_userNotificationManager.get(),
            m_timeNotificationManager.get(),
            m_businessEventNotificationManager.get(),
            m_layoutNotificationManager.get(),
            m_layoutTourNotificationManager.get(),
            m_videowallNotificationManager.get(),
            m_webPageNotificationManager.get(),
            m_storedFileNotificationManager.get(),
            m_updatesNotificationManager.get(),
            m_miscNotificationManager.get(),
            m_discoveryNotificationManager.get(),
            m_analyticsNotificationManager.get()));

    m_auditManager.reset(new ECConnectionAuditManager(this));
}

template<class QueryProcessorType>
BaseEc2Connection<QueryProcessorType>::~BaseEc2Connection()
{
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::startReceivingNotifications()
{
    connect(m_connectionFactory->messageBus(), &AbstractTransactionMessageBus::peerFound,
        this, &BaseEc2Connection<QueryProcessorType>::remotePeerFound, Qt::DirectConnection);
    connect(m_connectionFactory->messageBus(), &AbstractTransactionMessageBus::peerLost,
        this, &BaseEc2Connection<QueryProcessorType>::remotePeerLost, Qt::DirectConnection);
    connect(m_connectionFactory->messageBus(), &AbstractTransactionMessageBus::remotePeerUnauthorized,
        this, &BaseEc2Connection<QueryProcessorType>::remotePeerUnauthorized, Qt::DirectConnection);
    connect(m_connectionFactory->messageBus(), &AbstractTransactionMessageBus::newDirectConnectionEstablished,
        this, &BaseEc2Connection<QueryProcessorType>::newDirectConnectionEstablished, Qt::DirectConnection);

    messageBus()->start();
    m_connectionFactory->timeSyncManager()->start();
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::stopReceivingNotifications()
{
    m_connectionFactory->timeSyncManager()->stop();
    m_connectionFactory->messageBus()->disconnectAndJoin(this);
    m_connectionFactory->messageBus()->stop();
}

template<class QueryProcessorType>
AbstractResourceManagerPtr BaseEc2Connection<QueryProcessorType>::makeResourceManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnResourceManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractResourceNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::resourceNotificationManager()
{
    return m_resourceNotificationManager;
}

template<class QueryProcessorType>
AbstractMediaServerManagerPtr BaseEc2Connection<QueryProcessorType>::makeMediaServerManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnMediaServerManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractMediaServerNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::mediaServerNotificationManager()
{
    return m_mediaServerNotificationManager;
}

template<class QueryProcessorType>
AbstractCameraManagerPtr BaseEc2Connection<QueryProcessorType>::makeCameraManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnCameraManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractCameraNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::cameraNotificationManager()
{
    return m_cameraNotificationManager;
}

template<class QueryProcessorType>
AbstractLicenseManagerPtr BaseEc2Connection<QueryProcessorType>::makeLicenseManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnLicenseManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractLicenseNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::licenseNotificationManager()
{
    return m_licenseNotificationManager;
}

template<class QueryProcessorType>
AbstractEventRulesManagerPtr BaseEc2Connection<QueryProcessorType>::makeEventRulesManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<EventRulesManager<QueryProcessorType>>(
        messageBus(), m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractBusinessEventNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::businessEventNotificationManager()
{
    return m_businessEventNotificationManager;
}

template<class QueryProcessorType>
AbstractUserManagerPtr BaseEc2Connection<QueryProcessorType>::makeUserManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnUserManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractUserNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::userNotificationManager()
{
    return m_userNotificationManager;
}

template<class QueryProcessorType>
AbstractLayoutManagerPtr BaseEc2Connection<QueryProcessorType>::makeLayoutManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnLayoutManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractLayoutTourManagerPtr BaseEc2Connection<QueryProcessorType>::makeLayoutTourManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnLayoutTourManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractLayoutNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::layoutNotificationManager()
{
    return m_layoutNotificationManager;
}

template<class QueryProcessorType>
AbstractLayoutTourNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::layoutTourNotificationManager()
{
    return m_layoutTourNotificationManager;
}

template<class QueryProcessorType>
AbstractVideowallManagerPtr BaseEc2Connection<QueryProcessorType>::makeVideowallManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnVideowallManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<typename QueryProcessorType>
AbstractVideowallNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::videowallNotificationManager()
{
    return m_videowallNotificationManager;
}

template<class QueryProcessorType>
QnCommonModule* BaseEc2Connection<QueryProcessorType>::commonModule() const
{
    return m_connectionFactory->commonModule();
}

template<class QueryProcessorType>
AbstractWebPageManagerPtr BaseEc2Connection<QueryProcessorType>::makeWebPageManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnWebPageManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractWebPageNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::webPageNotificationManager()
{
    return m_webPageNotificationManager;
}

template<class QueryProcessorType>
AbstractStoredFileManagerPtr BaseEc2Connection<QueryProcessorType>::makeStoredFileManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnStoredFileManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
}

template<typename QueryProcessorType>
AbstractStoredFileNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::storedFileNotificationManager()
{
    return m_storedFileNotificationManager;
}

template<class QueryProcessorType>
AbstractUpdatesNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::updatesNotificationManager()
{
    return m_updatesNotificationManager;
}

template<class QueryProcessorType>
AbstractUpdatesManagerPtr BaseEc2Connection<QueryProcessorType>::makeUpdatesManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnUpdatesManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData, messageBus());
}

template<class QueryProcessorType>
AbstractMiscManagerPtr BaseEc2Connection<QueryProcessorType>::makeMiscManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnMiscManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
}

template<class QueryProcessorType>
AbstractMiscNotificationManagerPtr
    BaseEc2Connection<QueryProcessorType>::miscNotificationManager()
{
    return m_miscNotificationManager;
}

template<class QueryProcessorType>
AbstractDiscoveryManagerPtr BaseEc2Connection<QueryProcessorType>::makeDiscoveryManager(
    const Qn::UserAccessData& userAccessData)
{
    return std::make_shared<QnDiscoveryManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
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
AbstractAnalyticsManagerPtr BaseEc2Connection<QueryProcessorType>::makeAnalyticsManager(
    const Qn::UserAccessData &userAccessData)
{
    return std::make_shared<AnalyticsManager<QueryProcessorType>>(
        m_queryProcessor, userAccessData);
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
int BaseEc2Connection<QueryProcessorType>::dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler)
{
const int reqID = generateRequestID();

auto queryDoneHandler =
    [reqID, handler](ErrorCode errorCode, const nx::vms::api::DatabaseDumpData& data)
    {
        nx::vms::api::DatabaseDumpData outData;
        if (errorCode == ErrorCode::ok)
            outData = data;
        handler->done(reqID, errorCode, outData);
    };
m_queryProcessor->getAccess(Qn::kSystemAccess).template processQueryAsync<
    std::nullptr_t, nx::vms::api::DatabaseDumpData, decltype(queryDoneHandler)>(
    ApiCommand::dumpDatabase,
    nullptr,
    queryDoneHandler);
return reqID;
}

template<class QueryProcessorType>
int BaseEc2Connection<QueryProcessorType>::dumpDatabaseToFileAsync(
const QString& dumpFilePath,
impl::SimpleHandlerPtr handler)
{
const int reqID = generateRequestID();

nx::vms::api::StoredFilePath dumpFilePathData;
dumpFilePathData.path = dumpFilePath;

auto queryDoneHandler =
    [reqID, handler](
    ErrorCode errorCode,
    const nx::vms::api::DatabaseDumpToFileData& /*dumpFileSize*/)
    {
        handler->done(reqID, errorCode);
    };
m_queryProcessor->getAccess(Qn::kSystemAccess).template processQueryAsync<
    nx::vms::api::StoredFilePath, nx::vms::api::DatabaseDumpToFileData, decltype(
        queryDoneHandler)>(
    ApiCommand::dumpDatabaseToFile,
    dumpFilePathData,
    queryDoneHandler);
return reqID;
}

template<class QueryProcessorType>
int BaseEc2Connection<QueryProcessorType>::restoreDatabaseAsync(
const nx::vms::api::DatabaseDumpData& data,
impl::SimpleHandlerPtr handler)
{
const int reqID = generateRequestID();

using namespace std::placeholders;
m_queryProcessor->getAccess(Qn::kSystemAccess).processUpdateAsync(
    ApiCommand::restoreDatabase,
    data,
    std::bind(std::mem_fn(&impl::SimpleHandler::done), handler, reqID, _1));

return reqID;
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::addRemotePeer(
    const QnUuid& id,
    nx::vms::api::PeerType peerType,
    const nx::utils::Url &_url)
{
    nx::utils::Url url(_url);
    QUrlQuery q;
    url.setQuery(q);
    m_connectionFactory->messageBus()->addOutgoingConnectionToPeer(id, peerType, url);
}

template<class QueryProcessorType>
void BaseEc2Connection<QueryProcessorType>::deleteRemotePeer(const QnUuid& id)
{
    m_connectionFactory->messageBus()->removeOutgoingConnectionFromPeer(id);
}

template<class QueryProcessorType>
QnUuid BaseEc2Connection<QueryProcessorType>::routeToPeerVia(
    const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const
{
    auto messageBus = m_connectionFactory->messageBus();
    return messageBus ? messageBus->routeToPeerVia(dstPeer, distance, knownPeerAddress) : QnUuid();
}

template<class QueryProcessorType>
TransactionMessageBusAdapter* BaseEc2Connection<QueryProcessorType>::messageBus() const
{
    return m_connectionFactory->messageBus();
}

template<class QueryProcessorType>
nx::vms::time_sync::AbstractTimeSyncManager* BaseEc2Connection<QueryProcessorType>::timeSyncManager() const
{
    return m_connectionFactory->timeSyncManager();
}

} // namespace ec2
