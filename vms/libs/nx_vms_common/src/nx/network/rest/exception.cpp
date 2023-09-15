// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exception.h"

namespace nx::network::rest {

Exception::Exception(Result result): m_result(std::move(result))
{
}

QString Exception::message() const
{
    return m_result.errorString.isEmpty()
        ? QString(NX_FMT("Error %1", QJson::serialized(m_result.error)))
        : m_result.errorString;
}

UnauthorizedException::UnauthorizedException(AuthResult authResult, QString message):
    Exception(Result(
        [authResult]()
        {
            switch (authResult)
            {
                case AuthResult::Auth_WrongLogin: return Result::NotFound;
                case AuthResult::Auth_WrongInternalLogin: return Result::NotFound;
                case AuthResult::Auth_WrongDigest: return Result::InvalidParameter;
                case AuthResult::Auth_WrongPassword: return Result::InvalidParameter;
                case AuthResult::Auth_PasswordExpired: return Result::InvalidParameter;
                case AuthResult::Auth_Forbidden: return Result::Forbidden;
                case AuthResult::Auth_LDAPConnectError: return Result::ServiceUnavailable;
                case AuthResult::Auth_CloudConnectError: return Result::ServiceUnavailable;
                case AuthResult::Auth_DisabledUser: return Result::Forbidden;
                case AuthResult::Auth_InvalidCsrfToken: return Result::InvalidParameter;
                case AuthResult::Auth_LockedOut: return Result::ServiceUnavailable;
                case AuthResult::Auth_WrongSessionToken: return Result::InvalidParameter;
                case AuthResult::Auth_WrongTicketToken: return Result::InvalidParameter;
                case AuthResult::Auth_DisabledBasicAndDigest: return Result::Forbidden;
                case AuthResult::Auth_ClashedLogin: return Result::Forbidden;
                default: return Result::CantProcessRequest;
            }
        }(),
        message.isEmpty() ? toErrorMessage(authResult) : std::move(message))),
    authResult(authResult)
{
}

UnauthorizedException::UnauthorizedException(Result result, AuthResult authResult):
    Exception(std::move(result)), authResult(authResult)
{
}

} // namespace nx::network::rest
