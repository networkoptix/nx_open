#include "hanwha_firmware.h"

#include <QtCore/QRegExp>
#include <QtCore/QStringList>

namespace nx {
namespace vms::server {
namespace plugins {

HanwhaFirmware::HanwhaFirmware(const QString& firmwareString)
{
    parse(firmwareString);
}

int HanwhaFirmware::majorVersion() const
{
    return m_majorVersion;
}
int HanwhaFirmware::minorVersion() const
{
    return m_minorVersion;
}
QString HanwhaFirmware::build() const
{
    return m_build;
}
bool HanwhaFirmware::operator==(const HanwhaFirmware& other) const
{
    return m_majorVersion == other.m_majorVersion
        && m_minorVersion == other.m_minorVersion
        && m_build == other.m_build;
}
bool HanwhaFirmware::operator>(const HanwhaFirmware& other) const
{
    if (m_majorVersion != other.m_majorVersion)
        return m_majorVersion > other.m_majorVersion;

    if (m_minorVersion != other.m_minorVersion)
        return m_minorVersion > other.m_minorVersion;

    if (m_build != other.m_build)
        return m_build > other.m_build;

    return false;
}

bool HanwhaFirmware::operator<(const HanwhaFirmware& other) const
{
    if (m_majorVersion != other.m_majorVersion)
        return m_majorVersion < other.m_majorVersion;

    if (m_minorVersion != other.m_minorVersion)
        return m_minorVersion < other.m_minorVersion;

    if (m_build != other.m_build)
        return m_build < other.m_build;

    return false;
}

bool HanwhaFirmware::operator<=(const HanwhaFirmware& other) const
{
    return (*this < other) || (*this == other);
}

bool HanwhaFirmware::operator>=(const HanwhaFirmware& other) const
{
    return (*this > other) || (*this == other);
}

void HanwhaFirmware::parse(const QString& firmwareString)
{
    const auto split = firmwareString.split(
        QRegExp("[\\._]"),
        QString::SplitBehavior::SkipEmptyParts);

    if (split.size() >= 1)
        m_majorVersion = parseMajorVersion(split[0]);

    if (split.size() >= 2)
        m_minorVersion = split[1].trimmed().toInt();

    if (split.size() >= 3)
        m_build = split[2].trimmed();
}

int HanwhaFirmware::parseMajorVersion(const QString& majorVersionString) const
{
    if (majorVersionString.startsWith('v'))
        return majorVersionString.mid(1).toInt();

    return majorVersionString.toInt();
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
