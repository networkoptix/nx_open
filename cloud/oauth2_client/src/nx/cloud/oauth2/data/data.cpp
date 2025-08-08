// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/cloud/oauth2/api/data.h>

namespace nx::cloud::oauth2::api {

std::optional<std::string> ServiceTokenClaimSet::subjectTyp() const
{
    return get<std::string>("subjTyp");
}

void ServiceTokenClaimSet::setSubjectTyp(const std::string& val)
{
    set("subjTyp", val);
}

} // namespace nx::cloud::oauth2::api
