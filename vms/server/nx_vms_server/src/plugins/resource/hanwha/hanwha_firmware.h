#pragma once

#include <QtCore/QString>

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaFirmware
{
public:
    HanwhaFirmware(const QString& firmwareString);

    int majorVersion() const;

    int minorVersion() const;

    QString build() const;

    bool operator==(const HanwhaFirmware& other) const;

    bool operator>(const HanwhaFirmware& other) const;

    bool operator<(const HanwhaFirmware& other) const;

    bool operator<=(const HanwhaFirmware& other) const;

    bool operator>=(const HanwhaFirmware& other) const;

private:
    void parse(const QString& firmwareString);

    int parseMajorVersion(const QString& majorVersion) const;

private:
    int m_majorVersion = 0;
    int m_minorVersion = 0;
    QString m_build;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
