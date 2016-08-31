
#pragma once

#include <fstream>
#include <memory>

#include <QtCore/QString>

#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>

#include "utils.h"


class MediaServerLauncher
{
public:
    MediaServerLauncher(const QString& tmpDir = QString());
    ~MediaServerLauncher();

    SocketAddress endpoint() const;
    void addSetting(const QString& name, const QString& value);
    bool start();
    bool stop();

private:
    std::ofstream m_configFile;
    nx::ut::utils::WorkDirResource m_workDirResource;
    SocketAddress m_serverEndpoint;
    QString m_configFilePath;
    nx::utils::thread m_mediaServerProcessThread;
};
