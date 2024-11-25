// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/exception.h>

#include "auth_result.h"
#include "result.h"

namespace nx::network::rest {

class NX_NETWORK_REST_API Exception: public nx::utils::Exception
{
public:
    Exception(Result result);

    QString message() const override;
    const Result& result() const
    {
        return m_result;
    }

    #define NX_REST_EXCEPTION_METHOD(NAME) \
        template<typename... Args> \
        static Exception NAME(Args&&... args) { return {Result::NAME(std::forward<Args>(args)...)}; }
    NX_REST_EXCEPTION_METHOD(missingParameter)
    NX_REST_EXCEPTION_METHOD(invalidParameter)
    NX_REST_EXCEPTION_METHOD(cantProcessRequest)
    NX_REST_EXCEPTION_METHOD(forbidden)
    NX_REST_EXCEPTION_METHOD(conflict)
    NX_REST_EXCEPTION_METHOD(badRequest)
    NX_REST_EXCEPTION_METHOD(notAllowed)
    NX_REST_EXCEPTION_METHOD(notImplemented)
    NX_REST_EXCEPTION_METHOD(notFound)
    NX_REST_EXCEPTION_METHOD(internalServerError)
    NX_REST_EXCEPTION_METHOD(unsupportedMediaType)
    NX_REST_EXCEPTION_METHOD(serviceUnavailable)
    NX_REST_EXCEPTION_METHOD(serviceUnauthorized)
    // NX_REST_EXCEPTION_METHOD(unauthorized) - Use UnauthorizedException with appropriate AuthResult.
    // NX_REST_EXCEPTION_METHOD(sessionExpired) - Use UnauthorizedException::sessionExpired.
    NX_REST_EXCEPTION_METHOD(sessionRequired)
    // NX_REST_EXCEPTION_METHOD(sessionTruncated) - Use UnauthorizedException::sessionTruncated.
    NX_REST_EXCEPTION_METHOD(gone)
    #undef NX_REST_EXCEPTION_METHOD

private:
    Result m_result;
};

class NX_NETWORK_REST_API UnauthorizedException: public Exception
{
public:
    AuthResult authResult;

    UnauthorizedException(AuthResult authResult, QString message = {});

    static UnauthorizedException wrongSessionToken(QString message = {})
    {
        return UnauthorizedException(AuthResult::Auth_WrongSessionToken, std::move(message));
    }

    static UnauthorizedException sessionExpired(QString message = {})
    {
        return UnauthorizedException(Result(Result::Error::SessionExpired, std::move(message)),
            AuthResult::Auth_WrongSessionToken);
    }

    static UnauthorizedException truncatedSessionToken(QString message = {})
    {
        return UnauthorizedException(Result(Result::Error::SessionTruncated, std::move(message)),
            AuthResult::Auth_TruncatedSessionToken);
    }

    static UnauthorizedException wrongTicketToken(QString message = {})
    {
        return UnauthorizedException(AuthResult::Auth_WrongTicketToken, std::move(message));
    }

private:
    UnauthorizedException(Result result, AuthResult authResult);
};

} // namespace nx::network::rest
