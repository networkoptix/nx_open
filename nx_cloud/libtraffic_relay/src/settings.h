#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/settings.h>
#include <nx/utils/log/log_settings.h>

#include <utils/common/command_line_parser.h>

namespace nx {
namespace cloud {
namespace relay {
namespace conf {

struct Http
{
    std::list<SocketAddress> endpointsToListen;
    /**
     * Backlog value to pass to tcpServerSocket->listen call.
     */
    int tcpBacklogSize;

    Http();
};

struct ListeningPeer
{
    int recommendedPreemptiveConnectionCount;
    int maxPreemptiveConnectionCount;

    ListeningPeer();
};

class Settings
{
public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    bool isShowHelpRequested() const;

    void load(int argc, char **argv);
    void printCmdLineArgsHelp();

    QString dataDir() const;

    const utils::log::Settings& logging() const;
    const ListeningPeer& listeningPeer() const;
    const Http& http() const;

private:
    QnCommandLineParser m_commandLineParser;
    QnSettings m_settings;
    bool m_showHelpRequested;
    utils::log::Settings m_logging;
    ListeningPeer m_listeningPeer;
    Http m_http;

    void loadSettings();
    void loadHttp();
    void loadListeningPeer();
};

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx
