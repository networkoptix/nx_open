#pragma once

#include <fstream>
#include <memory>
#include <list>

#include <QtCore/QString>

#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>

#include "utils.h"
#include "media_server_process.h"

class MediaServerLauncher: public QObject
{
    Q_OBJECT
public:
    MediaServerLauncher(const QString& tmpDir = QString(), bool optimizeStartupTime = true);
    ~MediaServerLauncher();

    SocketAddress endpoint() const;
    int port() const;

    void addSetting(const QString& name, const QVariant& value);

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
    QUrl apiUrl() const;
signals:
    void started();
private:
    void prepareToStart();

    std::ofstream m_configFile;
    nx::ut::utils::WorkDirResource m_workDirResource;
    SocketAddress m_serverEndpoint;
    QString m_configFilePath;
    //nx::utils::thread m_mediaServerProcessThread;
    std::unique_ptr<MediaServerProcess> m_mediaServerProcess;
    bool m_firstStartup;
    std::list<std::pair<QString, QString>> m_customSettings;
};
