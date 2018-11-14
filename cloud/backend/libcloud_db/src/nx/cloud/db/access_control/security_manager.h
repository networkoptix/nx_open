#pragma once

namespace nx::cloud::db {

class AuthenticationManager;
class AuthorizationManager;
class AccessBlocker;

class SecurityManager
{
public:
    SecurityManager(
        AuthenticationManager* authenticator,
        const AuthorizationManager& authorizer,
        const AccessBlocker& accessBlocker);

    AuthenticationManager& authenticator();
    const AuthenticationManager& authenticator() const;

    const AccessBlocker& accessBlocker() const;

    const AuthorizationManager& authorizer() const;

private:
    AuthenticationManager* m_authenticator;
    const AuthorizationManager& m_authorizer;
    const AccessBlocker& m_transportSecurityManager;
};

} // namespace nx::cloud::db
