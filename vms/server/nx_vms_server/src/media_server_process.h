#pragma once

#include <memory>

#include <QtCore/QTimer>
#include <QtCore/QStringList>

#include <api/common_message_processor.h>
#include <nx/vms/event/event_fwd.h>
#include <core/resource/resource_fwd.h>
#include <server/server_globals.h>

#include "http/auto_request_forwarder.h"
#include "http/progressive_downloading_server.h"
#include "network/universal_tcp_listener.h"
#include "platform/monitoring/global_monitor.h"
#include <platform/platform_abstraction.h>

#include "nx/utils/thread/long_runnable.h"
#include "nx_ec/impl/ec_api_impl.h"
#include <nx/network/public_ip_discovery.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/network/upnp/upnp_port_mapper.h>
#include <media_server/serverutil.h>
#include <media_server/media_server_module.h>

#include "health/system_health.h"
#include "platform/platform_abstraction.h"
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_settings.h>


#include <nx/vms/discovery/manager.h>
#include <common/static_common_module.h>
#include <managers/discovery_manager.h>
#include "core/multicast/multicast_http_server.h"
#include "nx/vms/server/hls/hls_session_pool.h"
#include "nx/vms/server/hls/hls_server.h"
#include <recorder/remote_archive_synchronizer.h>
#include <recorder/recording_manager.h>
#include <camera/camera_pool.h>
#include <plugins/resource/mdns/mdns_listener.h>
#include <nx/network/upnp/upnp_device_searcher.h>
#include <plugins/resource/mserver_resource_searcher.h>
#include <media_server/media_server_resource_searchers.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <motion/motion_helper.h>
#include "server/host_system_password_synchronizer.h"
#include <database/server_db.h>
#include <media_server/mserver_status_watcher.h>
#include <media_server/resource_status_watcher.h>
#include <media_server/server_connector.h>
#include <media_server/server_update_tool.h>
#include <streaming/audio_streamer_pool.h>
#include <media_server_process_aux.h>
#include <nx/vms/server/command_line_parameters.h>
#include <nx/vms/server/discovery/discovery_monitor.h>

class QnAppserverResourceProcessor;
class QNetworkReply;
class QnServerMessageProcessor;
struct QnPeerRuntimeInfo;
struct BeforeRestoreDbData;
class TimeBasedNonceProvider;
class CloudIntegrationManager;
class TcpLogReceiver;
class QnServerMessageProcessor;

namespace ec2 {

class CrashReporter;
class LocalConnectionFactory;

} // namespace ec2

namespace nx { namespace vms { namespace cloud_integration { class CloudManagerGroup; } } }

void restartServer(int restartTimeout);

class MediaServerProcess: public QnLongRunnable
{
    Q_OBJECT

public:
    MediaServerProcess(int argc, char* argv[], bool serviceMode = false);
    ~MediaServerProcess();

    void stopObjects();
    void run();
    int getTcpPort() const;

    /** Entry point. */
    static int main(int argc, char* argv[]);

    const nx::vms::server::CmdLineArguments cmdLineArguments() const;

    QnCommonModule* commonModule() const
    {
        if (const auto& module = m_serverModule.lock())
            return module->commonModule();
        else
            return nullptr;
    }

    QnMediaServerModule* serverModule() const
    {
        if (const auto& module = m_serverModule.lock())
            return module.get();
        else
            return nullptr;
    }

    nx::vms::server::Authenticator* authenticator() const { return m_universalTcpListener->authenticator(); }

    static void configureApiRestrictions(nx::network::http::AuthMethodRestrictionList* restrictions);

    bool enableMultipleInstances() const { return m_enableMultipleInstances; }
    void initStaticCommonModule();
    void setSetupModuleCallback(std::function<void(QnMediaServerModule*)> callback);
    bool installUpdateRequestReceived() const { return m_installUpdateRequestReceived; }

signals:
    void started();

public slots:
    void stopAsync();
    void stopSync();

private slots:
    void at_portMappingChanged(QString address);
    void at_serverSaved(int, ec2::ErrorCode err);
    void at_cameraIPConflict(const QHostAddress& host, const QStringList& macAddrList);
    void at_storageManager_noStoragesAvailable();
    void at_storageManager_storagesAvailable();
    void at_storageManager_storageFailure(const QnResourcePtr& storage,
        nx::vms::api::EventReason reason);
    void at_storageManager_rebuildFinished(QnSystemHealth::MessageType msgType);
    void at_archiveBackupFinished(qint64 backedUpToMs, nx::vms::api::EventReason code);
    void at_timer();
    void at_connectionOpened();
    void at_serverModuleConflict(nx::vms::discovery::ModuleEndpoint module);

    void at_appStarted();
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);
    void at_emptyDigestDetected(
        const QnUserResourcePtr& user, const QString& login, const QString& password);
    void at_databaseDumped();
    void at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void at_updatePublicAddress(const QHostAddress& publicIp);
    void at_metadataStorageIdChanged(const QnResourcePtr& resource);

private:
    void updateDisabledVendorsIfNeeded();
    void updateAllowCameraChangesIfNeeded();
    void moveHandlingCameras();
    void updateAddressesList();
    void initStoragesAsync(QnCommonMessageProcessor* messageProcessor);

    void registerRestHandlers(
        nx::vms::cloud_integration::CloudManagerGroup* const cloudManagerGroup,
        QnUniversalTcpListener* tcpListener,
        ec2::TransactionMessageBusAdapter* messageBus);

    /**
     * This weird name is an apidoctool requirement.
     */
    void reg(
        const QString& path,
        QnRestRequestHandler* handler,
        GlobalPermission permission = GlobalPermission::none);

    void reg(
        const nx::network::http::Method::ValueType& method,
        const QString& path,
        QnRestRequestHandler* handler,
        GlobalPermission permission = GlobalPermission::none);

    template<class TcpConnectionProcessor, typename... ExtraParam>
    void regTcp(const QByteArray& protocol, const QString& path, ExtraParam... extraParam);

    bool initTcpListener(
        TimeBasedNonceProvider* timeBasedNonceProvider,
        nx::vms::cloud_integration::CloudManagerGroup* const cloudManagerGroup,
        ec2::LocalConnectionFactory* ec2ConnectionFactory);
    void initializeCloudConnect();
    void prepareOsResources();

    void initializeUpnpPortMapper();
    nx::vms::api::ServerFlags calcServerFlags();
    void initPublicIpDiscovery();
    void startPublicIpDiscovery();
    QnMediaServerResourcePtr findServer(ec2::AbstractECConnectionPtr ec2Connection);
    void saveStorages(
        ec2::AbstractECConnectionPtr ec2Connection, const QnStorageResourceList& storages);
    void dumpSystemUsageStats();
    bool isStopping() const;
    void resetSystemState(nx::vms::cloud_integration::CloudConnectionManager& cloudConnectionManager);
    void performActionsOnExit();
    void updateAllowedInterfaces();
    void addCommandLineParametersFromConfig(MSSettings* settings);
    void saveServerInfo(const QnMediaServerResourcePtr& server);

    nx::utils::log::Settings makeLogSettings(const nx::vms::server::Settings& settings);

    void initializeLogging(MSSettings* serverSettings);
    void initializeHardwareId();
    QString hardwareIdAsGuid() const;
    void updateGuidIfNeeded();
    void connectArchiveIntegrityWatcher();
    QnMediaServerResourcePtr registerServer(
        ec2::AbstractECConnectionPtr ec2Connection,
        const QnMediaServerResourcePtr &server,
        bool isNewServerInstance);
    nx::utils::Url appServerConnectionUrl() const;
    bool setUpMediaServerResource(
        CloudIntegrationManager* cloudIntegrationManager,
        QnMediaServerModule* serverModule,
        const ec2::AbstractECConnectionPtr& ec2Connection);
    ec2::AbstractECConnectionPtr createEc2Connection() const;
    bool connectToDatabase();
    void migrateDataFromOldDir();
    void initCrashDump();
    void initSsl();
    void setUpServerRuntimeData();
    void doMigrationFrom_2_4();
    void loadPlugins();
    void initResourceTypes();

    QnStorageResourceList updateStorages(QnMediaServerResourcePtr mServer);
    QnStorageResourceList createStorages(const QnMediaServerResourcePtr& mServer);
    QnStorageResourcePtr createStorage(const QnUuid& serverId, const QString& path);
    QStringList listRecordFolders(bool includeNonHdd = false) const;
    void connectSignals();
    void connectStorageSignals(QnStorageManager* storage);
    void setUpDataFromSettings();
    bool initializeAnalyticsEvents();
    QnUuid selectDefaultStorageForAnalyticsEvents(QnMediaServerResourcePtr server);
    void setUpTcpLogReceiver();
    void initNewSystemStateIfNeeded(
        bool foundOwnServerInDb,
        const nx::mserver_aux::SettingsProxyPtr& settingsProxy);
    void startObjects();
    std::map<QString, QVariant> confParamsFromSettings() const;
    void writeMutableSettingsData();
    void createTcpListener();
    void loadResourcesFromDatabase();
    void updateRootPassword();
    void createResourceProcessor();
    void setRuntimeFlag(nx::vms::api::RuntimeFlag flag, bool isSet);
    void loadResourceParamsData();
    void removeDatabase(const QString& databasePath) const;
private:
    int m_argc = 0;
    char** m_argv = nullptr;
    nx::vms::server::CmdLineArguments m_cmdLineArguments;
    const bool m_serviceMode;
    quint64 m_dumpSystemResourceUsageTaskId = 0;
    bool m_stopping = false;
    QnMutex m_mutex;
    mutable QnMutex m_stopMutex;
    std::map<nx::network::HostAddress, quint16> m_forwardedAddresses;
    QSet<QnUuid> m_updateUserRequests;
    std::unique_ptr<QnPlatformAbstraction> m_platform;
    std::unique_ptr<nx::utils::promise<void>> m_initStoragesAsyncPromise;
    bool m_enableMultipleInstances = false;
    QnMediaServerResourcePtr m_mediaServer;
    std::unique_ptr<QTimer> m_generalTaskTimer;
    std::unique_ptr<QTimer> m_udtInternetTrafficTimer;
    std::unique_ptr<QTimer> m_createDbBackupTimer;
    QVector<QString> m_hardwareIdHlist;
    QnServerMessageProcessor* m_serverMessageProcessor = nullptr;
    QString m_oldAnalyticsStoragePath;

    static std::unique_ptr<QnStaticCommonModule> m_staticCommonModule;
    std::weak_ptr<QnMediaServerModule> m_serverModule;

    std::unique_ptr<QnAutoRequestForwarder> m_autoRequestForwarder;
    std::unique_ptr<QnUniversalTcpListener> m_universalTcpListener;
    std::unique_ptr<ec2::LocalConnectionFactory> m_ec2ConnectionFactory;

    std::unique_ptr<nx::network::PublicIPDiscovery> m_ipDiscovery;
    std::unique_ptr<QTimer> m_updatePiblicIpTimer;
    std::unique_ptr<ec2::CrashReporter> m_crashReporter;

    std::unique_ptr<nx::vms::server::discovery::DiscoveryMonitor> m_discoveryMonitor;
    ec2::AbstractECConnectionPtr m_ec2Connection;
    std::unique_ptr<QnMulticast::HttpServer> m_multicastHttp;
    std::unique_ptr<nx::vms::server::recorder::RemoteArchiveSynchronizer> m_remoteArchiveSynchronizer;
    std::unique_ptr<QnMServerResourceSearcher> m_mserverResourceSearcher;
    std::unique_ptr<QnAppserverResourceProcessor> m_serverResourceProcessor;
    std::unique_ptr<TimeBasedNonceProvider> m_timeBasedNonceProvider;
    std::unique_ptr<CloudIntegrationManager> m_cloudIntegrationManager;

    std::unique_ptr<MediaServerStatusWatcher> m_mediaServerStatusWatcher;
    std::unique_ptr<QnAudioStreamerPool> m_audioStreamerPool;
    std::shared_ptr<TcpLogReceiver> m_logReceiver;
    std::unique_ptr<nx::network::upnp::PortMapper> m_upnpPortMapper;
    std::function<void(QnMediaServerModule*)> m_setupModuleCallback;

    std::atomic<bool> m_installUpdateRequestReceived{false};
};
