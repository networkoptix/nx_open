#pragma once

#include <map>
#include <vector>

#include "../http_types.h"

namespace nx {
namespace network {
namespace http {

template<typename Mapped>
class ExactPathMatcher
{
public:
    bool add(const std::string& path, Mapped mapped)
    {
        return m_pathToMapped.emplace(path, std::move(mapped)).second;
    }

    boost::optional<const Mapped&> match(
        const std::string& path,
        RequestPathParams* /*pathParams*/) const
    {
        auto it = m_pathToMapped.find(path);
        if (it == m_pathToMapped.end())
            return boost::none;

        return boost::optional<const Mapped&>(it->second);
    }

private:
    std::map<std::string, Mapped> m_pathToMapped;
};

} // namespace nx
} // namespace network
} // namespace http
