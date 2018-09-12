#pragma once

namespace nx::cdb {

class AuthenticationManager;
class AuthorizationManager;
class AccessBlocker;

class SecurityManager
{
public:
    SecurityManager(
        AuthenticationManager* authenticator,
        const AuthorizationManager& authorizer,
        const AccessBlocker& transportSecurityManager);

    AuthenticationManager& authenticator();
    const AuthenticationManager& authenticator() const;

    const AccessBlocker& transportSecurityManager() const;

    const AuthorizationManager& authorizer() const;

private:
    AuthenticationManager* m_authenticator;
    const AuthorizationManager& m_authorizer;
    const AccessBlocker& m_transportSecurityManager;
};

} // namespace nx::cdb
