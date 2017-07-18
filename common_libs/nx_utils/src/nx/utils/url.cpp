#include "url.h"

namespace nx {
namespace utils {
namespace url {

bool equal(const QUrl& lhs, const QUrl& rhs, ComparisonFlags flags)
{
    if (flags.testFlag(ComparisonFlag::All))
        return lhs == rhs;

    return (!flags.testFlag(ComparisonFlag::Host) || lhs.host() == rhs.host())
        && (!flags.testFlag(ComparisonFlag::Port) || lhs.port() == rhs.port())
        && (!flags.testFlag(ComparisonFlag::Scheme) || lhs.scheme() == rhs.scheme())
        && (!flags.testFlag(ComparisonFlag::UserName) || lhs.userName() == rhs.userName())
        && (!flags.testFlag(ComparisonFlag::Password) || lhs.password() == rhs.password())
        && (!flags.testFlag(ComparisonFlag::Path) || lhs.path() == rhs.path())
        && (!flags.testFlag(ComparisonFlag::Fragment) || lhs.fragment() == rhs.fragment())
        && (!flags.testFlag(ComparisonFlag::Query) || lhs.query() == rhs.query());
}

} // namespace url
} // namespace utils
} // namespace nx
