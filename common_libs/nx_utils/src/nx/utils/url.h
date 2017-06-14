#pragma once

#include <QtCore/QUrl>

namespace nx {
namespace utils {
namespace url {

enum class UrlPart
{
    Scheme = 0x01,
    UserName = 0x02,
    Password = 0x04,
    Authority = UserName | Password,
    Host = 0x08,
    Port = 0x10,
    Address = Host | Port,
    Path = 0x20,
    Fragment = 0x40,
    Query = 0x80,
    All = Scheme | Authority | Address | Path | Fragment | Query
};

using ComparisonFlag = UrlPart;
Q_DECLARE_FLAGS(ComparisonFlags, ComparisonFlag)

NX_UTILS_API bool equal(const QUrl& lhs, const QUrl& rhs, ComparisonFlags flags = ComparisonFlag::All);

inline bool addressesEqual(const QUrl& lhs, const QUrl& rhs)
{
    return equal(lhs, rhs, ComparisonFlag::Address);
}

} // namespace url
} // namespace utils
} // namespace nx
