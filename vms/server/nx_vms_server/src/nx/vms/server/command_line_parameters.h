#pragma once

#include <QtCore/QString>

namespace nx {

namespace utils { class ArgumentParser; }

namespace vms::server {

class CmdLineArguments
{
public:
    CmdLineArguments(const QStringList& arguments);
    CmdLineArguments(int argc, char* argv[]);

    QString logLevel;
    QString httpLogLevel;
    QString systemLogLevel;
    QString ec2TranLogLevel;
    QString permissionsLogLevel;

    QString rebuildArchive;
    QString allowedDiscoveryPeers;
    QString ifListFilter;
    bool cleanupDb = false;
    bool moveHandlingCameras = false;

    QString configFilePath;
    QString rwConfigFilePath;
    bool showVersion = false;
    bool showHelp = false;
    QString engineVersion;
    QString enforceSocketType;
    QString enforcedMediatorEndpoint;
    QString ipVersion;
    QString createFakeData;
    QString crashDirectory;
    std::vector<QString> auxLoggers;
private:
    void init(const QStringList& arguments);
};

} // namespace vms::server
} // namespace nx
