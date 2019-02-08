#include "software_version.h"

#include <algorithm>

#include <QtCore/QStringList>

namespace nx {
namespace utils {

SoftwareVersion::SoftwareVersion()
{
    m_data.fill(0);
}

SoftwareVersion::SoftwareVersion(int major, int minor, int bugfix /* = 0*/, int build /* = 0*/) :
    m_data{{major, minor, bugfix, build}}
{
}

SoftwareVersion::SoftwareVersion(const QString& versionString):
    SoftwareVersion()
{
    // Implementation differs a little bit from QnLexical conventions.
    // We try to set target to some sane value regardless of whether
    // deserialization has failed or not. We also support OpenGL-style
    // extended versions.

    QString s = versionString.trimmed();
    int index = s.indexOf(L' ');
    if (index != -1)
        s = s.mid(0, index);

    QStringList versionList = s.split(QLatin1Char('.'));
    for (int i = 0, count = std::min<int>(static_cast<int>(m_data.size()), versionList.size()); i < count; i++)
        m_data[i] = versionList[i].toInt();
}

SoftwareVersion::SoftwareVersion(const char* versionString):
    SoftwareVersion(QLatin1String(versionString))
{
}

SoftwareVersion::SoftwareVersion(const QByteArray& versionString):
    SoftwareVersion(QString::fromUtf8(versionString))
{
}

QString SoftwareVersion::toString(SoftwareVersion::Format format) const
{
    QString result = QString::number(m_data[0]);
    for (int i = 1; i < format; i++)
        result += QLatin1Char('.') + QString::number(m_data[i]);
    return result;
}

bool SoftwareVersion::isNull() const
{
    return m_data[0] == 0 && m_data[1] == 0 && m_data[2] == 0 && m_data[3] == 0;
}

bool operator<(const SoftwareVersion& l, const SoftwareVersion& r)
{
    return std::lexicographical_compare(l.m_data.begin(), l.m_data.end(),
        r.m_data.begin(), r.m_data.end());
}

bool operator==(const SoftwareVersion& l, const SoftwareVersion& r)
{
    return std::equal(l.m_data.begin(), l.m_data.end(), r.m_data.begin());
}

} // namespace utils
} // namespace nx
