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
    bool add(const nx::network::http::StringType& path, Mapped mapped)
    {
        return m_pathToMapped.emplace(path, std::move(mapped)).second;
    }

    boost::optional<const Mapped&> match(
        const nx::network::http::StringType& path,
        std::vector<nx::network::http::StringType>* /*pathParams*/) const
    {
        auto it = m_pathToMapped.find(path);
        if (it == m_pathToMapped.end())
            return boost::none;

        return boost::optional<const Mapped&>(it->second);
    }

private:
    std::map<nx::network::http::StringType, Mapped> m_pathToMapped;
};

} // namespace nx
} // namespace network
} // namespace http
