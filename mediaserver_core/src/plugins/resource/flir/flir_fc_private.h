#pragma once

namespace nx {
namespace plugins {
namespace flir {
namespace fc_private {

const QString kNexusInterfaceGroupName("INTERFACE Configuration - Device 0");
const QString kNexusPortParamName("Port");
const QString kConfigurationFile("/api/server/status/full");
const QString kStartNexusServerCommand("/api/server/start");

struct ServerStatus
{
    using ServerSettings = std::map<QString, std::map<QString, QString>>;

    ServerSettings settings;
    bool isNexusServerEnabled;
};

struct DeviceInfo
{
    QString model;
    QString serialNumber;
    QUrl url;
};

} // namespace fc_private
} // namespace flir 
} // namespace plugins
} // namespace nx