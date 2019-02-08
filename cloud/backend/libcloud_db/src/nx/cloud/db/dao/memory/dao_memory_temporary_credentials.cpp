#include "dao_memory_temporary_credentials.h"

#include <chrono>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <nx/utils/time.h>

namespace nx::cloud::db::dao::memory {

void TemporaryCredentialsDao::insert(
    nx::sql::QueryContext* /*queryContext*/,
    const nx::utils::stree::ResourceNameSet& /*attributeNameset*/,
    const data::TemporaryAccountCredentialsEx& tempPasswordData)
{
    m_credentialsById.emplace(tempPasswordData.id, tempPasswordData);
}

void TemporaryCredentialsDao::removeByAccountEmail(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& email)
{
    removeByCondition(
        [&email](const auto& credentials) { return credentials.accountEmail == email; });
}

std::optional<data::Credentials> TemporaryCredentialsDao::find(
    nx::sql::QueryContext* /*queryContext*/,
    const nx::utils::stree::ResourceNameSet& /*attributeNameset*/,
    const data::TemporaryAccountCredentials& filter)
{
    for (const auto& idAndCredentials: m_credentialsById)
    {
        if (idAndCredentials.second.login == filter.login)
        {
            return data::Credentials{
                idAndCredentials.second.login,
                idAndCredentials.second.password};
        }
    }

    return std::nullopt;
}

void TemporaryCredentialsDao::update(
    nx::sql::QueryContext* /*queryContext*/,
    const nx::utils::stree::ResourceNameSet& /*attributeNameset*/,
    const data::Credentials& credentials,
    const data::TemporaryAccountCredentials& tempPasswordData)
{
    for (auto& idAndCredentials: m_credentialsById)
    {
        if (idAndCredentials.second.login == credentials.login)
        {
            idAndCredentials.second.expirationTimestampUtc =
                tempPasswordData.expirationTimestampUtc;
            idAndCredentials.second.useCount = tempPasswordData.useCount;
            idAndCredentials.second.maxUseCount = tempPasswordData.maxUseCount;
        }
    }
}

void TemporaryCredentialsDao::deleteById(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& id) 
{
    m_credentialsById.erase(id);
}

void TemporaryCredentialsDao::deleteExpiredCredentials(
    nx::sql::QueryContext* /*queryContext*/)
{
    const auto currentTime = nx::utils::timeSinceEpoch();

    removeByCondition(
        [currentTime](const auto& credentials)
        {
            const bool isExpired =
                (credentials.expirationTimestampUtc > 0) &&
                (credentials.expirationTimestampUtc <= currentTime.count());

            return isExpired
                || credentials.useCount >= credentials.maxUseCount;
        });
}

std::vector<data::TemporaryAccountCredentialsEx> TemporaryCredentialsDao::fetchAll(
    nx::sql::QueryContext* /*queryContext*/,
    const nx::utils::stree::ResourceNameSet& /*attributeNameset*/)
{
    std::vector<data::TemporaryAccountCredentialsEx> result;
    std::transform(
        m_credentialsById.begin(), m_credentialsById.end(),
        std::back_inserter(result),
        [](const auto& idAndCredentials) { return idAndCredentials.second; });

    return result;
}

void TemporaryCredentialsDao::parsePasswordString(
    const std::string& passwordString,
    std::string* passwordHa1,
    std::string* password)
{
    std::vector<std::string> passwordParts;
    boost::algorithm::split(
        passwordParts,
        passwordString,
        boost::algorithm::is_any_of(":"),
        boost::algorithm::token_compress_on);
    if (passwordParts.size() >= 1)
        *passwordHa1 = passwordParts[0];
    if (passwordParts.size() >= 2)
        *password = passwordParts[1];
}

std::string TemporaryCredentialsDao::preparePasswordString(
    const std::string& passwordHa1,
    const std::string& password)
{
    std::vector<std::string> passwordParts;
    passwordParts.reserve(2);
    passwordParts.push_back(passwordHa1);
    if (!password.empty())
        passwordParts.push_back(password);
    return boost::algorithm::join(passwordParts, ":");
}

template<typename Condition>
void TemporaryCredentialsDao::removeByCondition(Condition conditionFunc)
{
    for (auto it = m_credentialsById.begin(); it != m_credentialsById.end();)
    {
        if (conditionFunc(it->second))
            it = m_credentialsById.erase(it);
        else
            ++it;
    }
}

} // namespace nx::cloud::db::dao::memory
