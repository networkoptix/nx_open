#pragma once

#include <utils/common/command_line_parser.h>
#include <utils/common/settings.h>

namespace nx {
namespace time_server {
namespace conf {

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
    const QnLogSettings& logging() const;

private:
    QnCommandLineParser m_commandLineParser;
    QnSettings m_settings;
    bool m_showHelpRequested;

    QnLogSettings m_logging;
};

} // namespace conf
} // namespace time_server
} // namespace nx
