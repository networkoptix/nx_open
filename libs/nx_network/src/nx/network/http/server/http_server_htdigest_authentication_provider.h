// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "http_server_base_authentication_manager.h"

#include <string>

namespace nx::network::http::server {

/**
 * Provides htdigest authentication by loading htdigest user/password combinations from a file or
 * input stream.
 * ABNF syntax for the file is following:  
 * <pre><code>
 * HTDIGEST_FILE_CONTENTS = 1 *( USER_RECORD LF )
 * USER_RECORD = USERNAME ":" REALM ":" HA1
 * USERNAME = TEXT
 * REALM = TEXT
 * HA1 = MD5( USERNAME ":" REALM ":" PASSWORD )
 * PASSWORD = TEXT
 * </code></pre>
 */
class NX_NETWORK_API HtdigestAuthenticationProvider: public AbstractAuthenticationDataProvider
{
public:
    HtdigestAuthenticationProvider(const std::string& filePath);
    HtdigestAuthenticationProvider(std::istream& input);

    virtual void getPasswordByUserName(
        const std::string& userName,
        AbstractAuthenticationDataProvider::LookupResultHandler completionHandler) override;

private:
    void load(std::istream& input);

private:
    nx::Mutex m_mutex;
    std::map<std::string, std::string> m_credentials;
};

} // namespace nx::network::http::server
