// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exception.h"

namespace nx::network::rest {

Exception::Exception(Result result): m_result(std::move(result))
{
}

QString Exception::message() const
{
    return m_result.errorString.isEmpty()
        ? QString(NX_FMT("Error %1", QJson::serialized(m_result.errorId)))
        : m_result.errorString;
}

UnauthorizedException::UnauthorizedException(AuthResult authResult, QString message):
    Exception(Result(
        [authResult]()
        {
            switch (authResult)
            {
                case AuthResult::Auth_WrongLogin: return ErrorId::notFound;
                case AuthResult::Auth_WrongInternalLogin: return ErrorId::notFound;
                case AuthResult::Auth_WrongDigest: return ErrorId::invalidParameter;
                case AuthResult::Auth_WrongPassword: return ErrorId::invalidParameter;
                case AuthResult::Auth_PasswordExpired: return ErrorId::invalidParameter;
                case AuthResult::Auth_Forbidden: return ErrorId::forbidden;
                case AuthResult::Auth_LDAPConnectError: return ErrorId::serviceUnavailable;
                case AuthResult::Auth_CloudConnectError: return ErrorId::serviceUnavailable;
                case AuthResult::Auth_DisabledUser: return ErrorId::forbidden;
                case AuthResult::Auth_InvalidCsrfToken: return ErrorId::invalidParameter;
                case AuthResult::Auth_LockedOut: return ErrorId::serviceUnavailable;
                case AuthResult::Auth_WrongSessionToken: return ErrorId::invalidParameter;
                case AuthResult::Auth_TruncatedSessionToken: return ErrorId::sessionTruncated;
                case AuthResult::Auth_WrongTicketToken: return ErrorId::invalidParameter;
                case AuthResult::Auth_DisabledBasicAndDigest: return ErrorId::forbidden;
                case AuthResult::Auth_ClashedLogin: return ErrorId::forbidden;
                default: return ErrorId::cantProcessRequest;
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
