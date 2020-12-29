#pragma once

#include <map>

#include <nx/utils/thread/mutex.h>

#include "http_server_base_authentication_manager.h"
#include "../auth_tools.h"

namespace nx {
namespace network {
namespace http {
namespace server {

class NX_NETWORK_API PlainTextCredentialsProvider:
    public AbstractAuthenticationDataProvider
{
public:
    virtual void getPasswordByUserName(
        const nx::String& userName,
        LookupResultHandler completionHandler) override;

    void addCredentials(const Credentials& credentials);

private:
    /** map<username, credentials>. */
    std::map<nx::String, Credentials> m_credentials;
    mutable QnMutex m_mutex;
};

} // namespace server
} // namespace nx
} // namespace network
} // namespace http
