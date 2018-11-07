
#pragma once

#include <array>

#include <QString>

namespace nx {
namespace mediaserver {
namespace api {

class VersionHolder
{
public:
    VersionHolder();

    VersionHolder(int major
        , int minor
        , int bugfix
        , int build);

    explicit VersionHolder(const QString &str);

    QString toString() const;

    bool operator < (const VersionHolder &other) const;

    bool operator <= (const VersionHolder &other) const;

    bool operator > (const VersionHolder &other) const;

    bool operator >= (const VersionHolder &other) const;

    bool operator == (const VersionHolder &other) const;

    bool operator != (const VersionHolder &other) const;

private:
    typedef std::array<int, 4> Data;

    Data m_data;
};

} // namespace api
} // namespace mediaserver
} // namespace nx
