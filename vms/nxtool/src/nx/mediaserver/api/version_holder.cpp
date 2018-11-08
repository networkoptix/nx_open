
#include "version_holder.h"

#include <QStringList>

namespace nx {
namespace mediaserver {
namespace api {

namespace {

template<typename ResultType>
ResultType initData(int major, int minor, int bugfix, int build)
{
    ResultType result;
    if (result.size() > 0)
        result[0] = major;
    
    if (result.size() > 1)
        result[1] = minor;

    if (result.size() > 2)
        result[2] = bugfix;

    if (result.size() > 3)
        result[3] = build;

    return result;
}

} // namespace

VersionHolder::VersionHolder()
    : m_data(initData<Data>(0, 0, 0, 0))
{
}

VersionHolder::VersionHolder(int major
    , int minor
    , int bugfix
    , int build)
    : m_data(initData<Data>(major, minor, bugfix, build))
{
}

VersionHolder::VersionHolder(const QString &str)
    : m_data(initData<Data>(0, 0, 0, 0))
{
    static const char kBetaDelim = ' ';
    const auto parts = str.trimmed().split(kBetaDelim);
    if (parts.isEmpty())
        return;

    const auto &version = parts.front();

    static const char kDotDelim = '.';
    const auto versionParts = version.trimmed().split(kDotDelim);

    const auto setData = [this](const int index, int value)
        { m_data[index] = value; };

    const std::size_t count = (versionParts.size() > m_data.size() ? m_data.size() : versionParts.size());
    for (int i = 0; i != count; ++i)
        setData(i, versionParts.at(i).toInt());
}

QString VersionHolder::toString() const
{
    static const auto kTemplate = QStringLiteral("%1.%2.%3.%4");
    return kTemplate.arg(QString::number(m_data[0])
        , QString::number(m_data[1])
        , QString::number(m_data[2])
        , QString::number(m_data[3]));
}

bool VersionHolder::operator < (const VersionHolder &other) const
{
    return (m_data < other.m_data);
}

bool VersionHolder::operator <= (const VersionHolder &other) const
{
    return (m_data <= other.m_data);
}

bool VersionHolder::operator > (const VersionHolder &other) const
{
    return (m_data > other.m_data);
}

bool VersionHolder::operator >= (const VersionHolder &other) const
{
    return (m_data >= other.m_data);
}

bool VersionHolder::operator == (const VersionHolder &other) const
{
    return (m_data == other.m_data);
}

bool VersionHolder::operator != (const VersionHolder &other) const
{
    return (m_data != other.m_data);
}

} // namespace api
} // namespace mediaserver
} // namespace nx
