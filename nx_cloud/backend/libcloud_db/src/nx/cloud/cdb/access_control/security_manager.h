#pragma once

namespace nx::cdb {

class AuthenticationManager;
class AuthorizationManager;
class TransportSecurityManager;

class SecurityManager
{
public:
    SecurityManager(
        AuthenticationManager* authenticator,
        const AuthorizationManager& authorizer,
        const TransportSecurityManager& transportSecurityManager);

    AuthenticationManager& authenticator();
    const AuthenticationManager& authenticator() const;

    const TransportSecurityManager& transportSecurityManager() const;

    const AuthorizationManager& authorizer() const;

private:
    AuthenticationManager* m_authenticator;
    const AuthorizationManager& m_authorizer;
    const TransportSecurityManager& m_transportSecurityManager;
};

} // namespace nx::cdb
