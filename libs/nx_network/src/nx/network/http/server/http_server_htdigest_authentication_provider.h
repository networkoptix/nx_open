#pragma once

#include "http_server_base_authentication_manager.h"

#include <string>

namespace nx::network::http::server {

/**
 * Provides htdigest authentication by loading htdigest user/password combinations from a file or
 * input stream.
 */
class NX_NETWORK_API HtdigestAuthenticationProvider: public AbstractAuthenticationDataProvider
{
public:
    HtdigestAuthenticationProvider(const std::string& filePath);
    HtdigestAuthenticationProvider(std::istream& input);

    virtual void getPasswordByUserName(
        const nx::String& userName,
        AbstractAuthenticationDataProvider::LookupResultHandler completionHandler) override;

private:
    void load(std::istream& input);

private:
    QnMutex m_mutex;
    std::map<nx::String, nx::String> m_credentials;
};

} // namespace nx::network::http::server