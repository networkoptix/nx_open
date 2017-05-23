#pragma once

#include <map>
#include <vector>

#include "../http_types.h"

namespace nx_http {

template<typename Mapped>
class ExactPathMatcher
{
public:
    bool add(const nx_http::StringType& path, Mapped mapped)
    {
        return m_pathToMapped.emplace(path, std::move(mapped)).second;
    }

    boost::optional<const Mapped&> match(
        const nx_http::StringType& path,
        std::vector<nx_http::StringType>* /*pathParams*/) const
    {
        auto it = m_pathToMapped.find(path);
        if (it == m_pathToMapped.end())
            return boost::none;

        return boost::optional<const Mapped&>(it->second);
    }

private:
    std::map<nx_http::StringType, Mapped> m_pathToMapped;
};

} // namespace nx_http
