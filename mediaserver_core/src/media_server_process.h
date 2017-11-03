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
#include <nx/vms/discovery/manager.h>

class QnAppserverResourceProcessor;
class QNetworkReply;
class QnServerMessageProcessor;
struct QnModuleInformation;
struct QnPeerRuntimeInfo;
class QnLdapManager;
struct BeforeRestoreDbData;
namespace ec2 { class CrashReporter; }

struct CloudManagerGroup;

void restartServer(int restartTimeout);

class CmdLineArguments
{
public:
    QString logLevel;
    QString exceptionFilters;

    QString httpLogLevel;
    QString hwLogLevel;
    QString ec2TranLogLevel;
    QString permissionsLogLevel;

    QString rebuildArchive;
    QString devModeKey;
    QString allowedDiscoveryPeers;
    QString ifListFilter;
    bool cleanupDb;
    bool moveHandlingCameras;

    QString configFilePath;
    QString rwConfigFilePath;
    bool showVersion;
    bool showHelp;
    QString engineVersion;
    QString enforceSocketType;
    QString enforcedMediatorEndpoint;
    QString ipVersion;
    QString createFakeData;

    CmdLineArguments():
        logLevel(
            #if defined(_DEBUG)
                lit("DEBUG")
            #else
                lit("INFO")
            #endif
        ),
        cleanupDb(false),
        moveHandlingCameras(false),
        showVersion(false),
        showHelp(false)
    {
    }
};

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

    void setHardwareGuidList(const QVector<QString>& hardwareGuidList);

    const CmdLineArguments cmdLineArguments() const;
    void setObsoleteGuid(const QnUuid& obsoleteGuid) { m_obsoleteGuid = obsoleteGuid; }

    QnCommonModule* commonModule() const
    {
        if (const auto& module = m_serverModule.lock())
            return module->commonModule();
        else
            return nullptr;
    }

    MSSettings* serverSettings() const { return m_settings.get(); }

    static void configureApiRestrictions(nx_http::AuthMethodRestrictionList* restrictions);

signals:
    void started();

public slots:
    void stopAsync();
    void stopSync();

private slots:
    void loadResourcesFromECS(
        ec2::AbstractECConnectionPtr ec2Connection,
        QnCommonMessageProcessor* messageProcessor);
    void at_portMappingChanged(QString address);
    void at_serverSaved(int, ec2::ErrorCode err);
    void at_cameraIPConflict(const QHostAddress& host, const QStringList& macAddrList);
    void at_storageManager_noStoragesAvailable();
    void at_storageManager_storageFailure(const QnResourcePtr& storage,
        nx::vms::event::EventReason reason);
    void at_storageManager_rebuildFinished(QnSystemHealth::MessageType msgType);
    void at_archiveBackupFinished(qint64 backedUpToMs, nx::vms::event::EventReason code);
    void at_timer();
    void at_connectionOpened();
    void at_serverModuleConflict(nx::vms::discovery::ModuleEndpoint module);

    void at_appStarted();
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);
    void at_timeChanged(qint64 time);
    void at_emptyDigestDetected(
        const QnUserResourcePtr& user, const QString& login, const QString& password);
    void at_databaseDumped();
    void at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void at_updatePublicAddress(const QHostAddress& publicIp);

private:
    void updateDisabledVendorsIfNeeded();
    void updateAllowCameraCHangesIfNeed();
    void moveHandlingCameras();
    void updateAddressesList();
    void initStoragesAsync(QnCommonMessageProcessor* messageProcessor);
    void registerRestHandlers(
        CloudManagerGroup* const cloudManagerGroup,
        QnUniversalTcpListener* tcpListener,
        ec2::TransactionMessageBusAdapter* messageBus);

    template<class TcpConnectionProcessor, typename... ExtraParam>
    void regTcp(const QByteArray& protocol, const QString& path, ExtraParam... extraParam);

    bool initTcpListener(
        CloudManagerGroup* const cloudManagerGroup,
        ec2::TransactionMessageBusAdapter* messageBus);
    void initializeCloudConnect();

    std::unique_ptr<nx_upnp::PortMapper> initializeUpnpPortMapper();
    Qn::ServerFlags calcServerFlags();
    void initPublicIpDiscovery();
    QnMediaServerResourcePtr findServer(ec2::AbstractECConnectionPtr ec2Connection);
    void saveStorages(
        ec2::AbstractECConnectionPtr ec2Connection, const QnStorageResourceList& storages);
    void dumpSystemUsageStats();
    bool isStopping() const;
    void resetSystemState(CloudConnectionManager& cloudConnectionManager);
    void performActionsOnExit();
    void parseCommandLineParameters(int argc, char* argv[]);
    void updateAllowedInterfaces();
    void addCommandLineParametersFromConfig(MSSettings* settings);
    void saveServerInfo(const QnMediaServerResourcePtr& server);

    void serviceModeInit();
    QString hardwareIdAsGuid() const;
    void updateGuidIfNeeded();

private:
    int m_argc;
    char** m_argv;
    bool m_startMessageSent;
    qint64 m_firstRunningTime;

    std::unique_ptr<QnAutoRequestForwarder> m_autoRequestForwarder;
    QnUniversalTcpListener* m_universalTcpListener;
    QnMediaServerResourcePtr m_mediaServer;
    QSet<QnUuid> m_updateUserRequests;
    std::map<HostAddress, quint16> m_forwardedAddresses;
    QnMutex m_mutex;
    std::unique_ptr<nx::network::PublicIPDiscovery> m_ipDiscovery;
    std::unique_ptr<QTimer> m_updatePiblicIpTimer;
    quint64 m_dumpSystemResourceUsageTaskID;
    bool m_stopping;
    mutable QnMutex m_stopMutex;
    std::unique_ptr<ec2::CrashReporter> m_crashReporter;
    QVector<QString> m_hardwareGuidList;
    nx::SystemName m_systemName;
    std::unique_ptr<QnPlatformAbstraction> m_platform;
    CmdLineArguments m_cmdLineArguments;
    QnUuid m_obsoleteGuid;
    std::unique_ptr<nx::utils::promise<void>> m_initStoragesAsyncPromise;
    std::weak_ptr<QnMediaServerModule> m_serverModule;
    bool m_serviceMode;
    std::unique_ptr<MSSettings> m_settings;
};
