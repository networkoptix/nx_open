// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <include/nx/cloud/db/api/oauth_data.h>

namespace nx::cloud::db::api {

std::optional<std::string> ClaimSet::clientId() const
{
    return get<std::string>("client_id");
}

void ClaimSet::setClientId(const std::string& val)
{
    set("client_id", val);
}

std::optional<std::string> ClaimSet::sid() const
{
    return get<std::string>("sid");
}

void ClaimSet::setSid(const std::string& val)
{
    set("sid", val);
}

std::optional<std::chrono::seconds> ClaimSet::passwordTime() const
{
    if (const auto val = get<int64_t>("pwdTime"); val.has_value())
        return std::chrono::seconds(*val);
    return std::nullopt;
}

void ClaimSet::setPasswordTime(const std::chrono::seconds& val)
{
    set("pwdTime", val.count());
}

std::optional<std::string> ClaimSet::typ() const
{
    return get<std::string>("typ");
}

void ClaimSet::setTyp(const std::string& val)
{
    set("typ", val);
}

std::optional<int> ClaimSet::securitySequence() const
{
    return get<int>("pwdSeq");
}

void ClaimSet::setSecuritySequence(int val)
{
    set("pwdSeq", val);
}

} // namespace nx::cloud::db::api
