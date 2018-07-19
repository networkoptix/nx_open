#pragma once

#include <map>
#include <string>

#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

namespace nx::cloud::relay::model {

class AliasManager
{
public:
    /**
     * @return Alias.
     */
    std::string addAliasToHost(const std::string& hostName);

    std::optional<std::string> findHostByAlias(const std::string& alias) const;

    void updateAlias(const std::string& alias, const std::string& hostName);

private:
    mutable QnMutex m_mutex;
    std::map<std::string, std::string> m_aliasToHostName;
};

} // namespace nx::cloud::relay::model
