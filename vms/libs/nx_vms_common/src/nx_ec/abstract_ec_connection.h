// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource_access/user_access_data.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/network/ssl/helpers.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/database_dump_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/id_data.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/reverse_connection_data.h>
#include <nx/vms/api/data/runtime_data.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/api/data/timestamp.h>
#include <nx/vms/time/abstract_time_sync_manager.h>

#include "ec_api_fwd.h"
#include "ec_api_common.h"

namespace ec2 {

class ECConnectionNotificationManager;
class TransactionMessageBusAdapter;
class QnAbstractTransactionTransport;
class ECConnectionAuditManager;

/*!
    \note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractECConnection: public QObject
{
    Q_OBJECT

public:
    virtual ~AbstractECConnection() = default;

    virtual nx::vms::api::ModuleInformation moduleInformation() const = 0;

    // TODO: #sivanov Remove from the base class.
    /** Address of the server we are currently connected to. Actual for the client side only. */
    virtual nx::network::SocketAddress address() const = 0;

    // TODO: #sivanov Remove from the base class.
    /** Credentials we are using to authorize the connection. Actual for the client side only. */
    virtual nx::network::http::Credentials credentials() const = 0;

    /** !Calling this method starts notifications delivery by emitting corresponding signals of
        the corresponding manager
        \note Calling entity MUST connect to all interesting signals prior to calling this method
        so that received data is consistent
    */
    virtual void startReceivingNotifications() = 0;
    virtual void stopReceivingNotifications() = 0;

    virtual void addRemotePeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url,
        nx::network::ssl::AdapterFunc adapterFunc = nx::network::ssl::kDefaultCertificateCheck) = 0;

    virtual void deleteRemotePeer(const QnUuid& id) = 0;

    virtual nx::vms::api::Timestamp getTransactionLogTime() const = 0;
    virtual void setTransactionLogTime(nx::vms::api::Timestamp value) = 0;
    virtual std::set<QnUuid> getRemovedObjects() const = 0;
    virtual bool resyncTransactionLog(const std::set<QnUuid>& filter = std::set<QnUuid>()) = 0;

    virtual AbstractResourceManagerPtr getResourceManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractMediaServerManagerPtr getMediaServerManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractCameraManagerPtr getCameraManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractLicenseManagerPtr getLicenseManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractEventRulesManagerPtr getEventRulesManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractVmsRulesManagerPtr getVmsRulesManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractUserManagerPtr getUserManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractLayoutManagerPtr getLayoutManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractLayoutTourManagerPtr getLayoutTourManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractVideowallManagerPtr getVideowallManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractStoredFileManagerPtr getStoredFileManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractMiscManagerPtr getMiscManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractDiscoveryManagerPtr getDiscoveryManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractWebPageManagerPtr getWebPageManager(
        const Qn::UserSession& userSession) = 0;
    virtual AbstractAnalyticsManagerPtr getAnalyticsManager(
        const Qn::UserSession& userSession) = 0;

    virtual AbstractResourceNotificationManagerPtr resourceNotificationManager() = 0;
    virtual AbstractMediaServerNotificationManagerPtr mediaServerNotificationManager() = 0;
    virtual AbstractCameraNotificationManagerPtr cameraNotificationManager() = 0;
    virtual AbstractLayoutNotificationManagerPtr layoutNotificationManager() = 0;
    virtual AbstractLayoutTourNotificationManagerPtr layoutTourNotificationManager() = 0;
    virtual AbstractVideowallNotificationManagerPtr videowallNotificationManager() = 0;
    virtual AbstractStoredFileNotificationManagerPtr storedFileNotificationManager() = 0;
    virtual AbstractMiscNotificationManagerPtr miscNotificationManager() = 0;
    virtual AbstractDiscoveryNotificationManagerPtr discoveryNotificationManager() = 0;
    virtual AbstractWebPageNotificationManagerPtr webPageNotificationManager() = 0;
    virtual AbstractAnalyticsNotificationManagerPtr analyticsNotificationManager() = 0;
    virtual AbstractLicenseNotificationManagerPtr licenseNotificationManager() = 0;
    virtual AbstractTimeNotificationManagerPtr timeNotificationManager() = 0;
    virtual AbstractBusinessEventNotificationManagerPtr businessEventNotificationManager() = 0;
    virtual AbstractVmsRulesNotificationManagerPtr vmsRulesNotificationManager() = 0;
    virtual AbstractUserNotificationManagerPtr userNotificationManager() = 0;

    virtual ECConnectionAuditManager* auditManager() = 0;

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer,
        int* distance,
        nx::network::SocketAddress* knownPeerAddress) const = 0;

    virtual TransactionMessageBusAdapter* messageBus() const = 0;
    virtual nx::vms::common::AbstractTimeSyncManagerPtr timeSyncManager() const = 0;

    virtual ECConnectionNotificationManager* notificationManager();

    virtual int dumpDatabase(
        Handler<nx::vms::api::DatabaseDumpData> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode dumpDatabaseSync(nx::vms::api::DatabaseDumpData* outData);

    virtual int dumpDatabaseToFile(
        const QString& dumpFilePath,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result dumpDatabaseToFileSync(const QString& dumpFilePath);

    virtual int restoreDatabase(
        const nx::vms::api::DatabaseDumpData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode restoreDatabaseSync(const nx::vms::api::DatabaseDumpData& data);

signals:
    void initNotification(const nx::vms::api::FullInfoData& fullData);
    void runtimeInfoChanged(const nx::vms::api::RuntimeData& runtimeInfo);
    void runtimeInfoRemoved(const nx::vms::api::IdData& runtimeInfo);

    void reverseConnectionRequested(const nx::vms::api::ReverseConnectionData& reverseConnetionData);

    void remotePeerFound(QnUuid data, nx::vms::api::PeerType peerType);
    void remotePeerLost(QnUuid data, nx::vms::api::PeerType peerType);
    void remotePeerUnauthorized(const QnUuid& id);
    void newDirectConnectionEstablished(QnAbstractTransactionTransport* transport);

    void settingsChanged(nx::vms::api::ResourceParamDataList settings);

    void databaseDumped();

    void serverRuntimeEventOccurred(
        const nx::vms::api::ServerRuntimeEventData& serverRuntimeEventData);
};

} // namespace ec2
