// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/utils/thread/mutex.h>

#include "http_server_base_authentication_manager.h"
#include "../auth_tools.h"

namespace nx::network::http::server {

class NX_NETWORK_API PlainTextCredentialsProvider:
    public AbstractAuthenticationDataProvider
{
public:
    virtual void getPasswordByUserName(
        const std::string& userName,
        LookupResultHandler completionHandler) override;

    void addCredentials(const Credentials& credentials);

private:
    /** map<username, credentials>. */
    std::map<std::string, Credentials> m_credentials;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::network::http::server
