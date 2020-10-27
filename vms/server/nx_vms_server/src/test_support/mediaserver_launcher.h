#pragma once

#include <fstream>
#include <memory>
#include <map>
#include <string>

#include <QtCore/QString>

#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>

#include "utils.h"
#include "../media_server_process.h"

class MediaServerLauncher: public QObject
{
    Q_OBJECT
public:

    enum MediaServerFeature
    {
        none                = 0,
        resourceDiscovery = 1 << 0,
        monitorStatistics = 1 << 1,
        storageDiscovery  = 1 << 2,
        plugins  = 1 << 3,
        publicIp  = 1 << 4,
        onlineResourceData = 1 << 5,
        outgoingConnectionsMetric = 1 << 6,
        useTwoSockets = 1 << 7,
        useSetupWizard = 1 << 8,
        count = 1 << 9,
        all = count - 1
    };
    Q_DECLARE_FLAGS(MediaServerFeatures, MediaServerFeature)


    MediaServerLauncher(
        const QString& tmpDir = QString(),
        int port = 0);
    ~MediaServerLauncher();

    /**
     * Turn some media server features on.
     * Can be called wnen server is not running. Before start() or after stop() call.
     */
    void addFeatures(MediaServerFeatures enabledFeatures);

    /**
     * Turn some media server features off.
     * Can be called wnen server is not running. Before start() or after stop() call.
     */
    void removeFeatures(MediaServerFeatures disabledFeatures);

    nx::network::SocketAddress endpoint() const;
    int port() const;
    QnCommonModule* commonModule() const;
    QnMediaServerModule* serverModule() const;
    nx::vms::server::Authenticator* authenticator() const;
    void connectTo(MediaServerLauncher* target, bool isSecure = true);

    void addSetting(const std::string& name, const QVariant& value);
    void addCmdOption(const std::string& option);

    /**
     * Run media server at the current thread
     */
    void run();

    /**
     * Start media server and wait while it's being started. Server started at separate thread.
     * MediaServerLauncher recreate media server do, so it starts with clean DB.
     * In case of successive start/stop/start calls server will use same DB.
     */
    bool start();

    bool waitForStarted();

    /**
     * Start media server. Don't wait while it's being started. Server started at separate thread.
     */
    bool startAsync();

    /** Stop media server process */
    bool stop();

    bool stopAsync();

    /**
     * Return media server API url
     */
    nx::utils::Url apiUrl() const;
    QString dataDir() const;
    MediaServerProcess* mediaServerProcess() const { return m_mediaServerProcess.get(); }

signals:
    void started();
private:
    void setLowDelayIntervals();
    void prepareToStart();
    void fillDefaultSettings();
    void setFeatures(MediaServerFeatures features, bool isEnabled);

    nx::ut::utils::WorkDirResource m_workDirResource;
    nx::network::SocketAddress m_serverEndpoint;
    QString m_configFilePath;
    //nx::utils::thread m_mediaServerProcessThread;
    std::unique_ptr<MediaServerProcess> m_mediaServerProcess;
    bool m_firstStartup;
    std::map<std::string, QString> m_settings;
    std::vector<std::string> m_cmdOptions;
    QnUuid m_serverGuid;
    std::unique_ptr<nx::utils::promise<bool>> m_processStartedPromise;
};
