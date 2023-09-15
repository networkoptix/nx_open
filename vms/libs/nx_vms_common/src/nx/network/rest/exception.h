// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/result.h>
#include <nx/utils/exception.h>
#include <nx/vms/auth/auth_result.h>

namespace nx::network::rest {

class NX_VMS_COMMON_API Exception: public nx::utils::Exception
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
    NX_REST_EXCEPTION_METHOD(notImplemented)
    NX_REST_EXCEPTION_METHOD(notFound)
    NX_REST_EXCEPTION_METHOD(internalServerError)
    NX_REST_EXCEPTION_METHOD(unsupportedMediaType)
    NX_REST_EXCEPTION_METHOD(serviceUnavailable)
    // NX_REST_EXCEPTION_METHOD(unauthorized) - Use UnauthorizedException with appropriate AuthResult.
    // NX_REST_EXCEPTION_METHOD(sessionExpired) - Use UnauthorizedException::sessionExpired.
    NX_REST_EXCEPTION_METHOD(sessionRequired)
    #undef NX_REST_EXCEPTION_METHOD

private:
    Result m_result;
};

class NX_VMS_COMMON_API UnauthorizedException: public Exception
{
public:
    using AuthResult = nx::vms::common::AuthResult;
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

    static UnauthorizedException wrongTicketToken(QString message = {})
    {
        return UnauthorizedException(AuthResult::Auth_WrongTicketToken, std::move(message));
    }

private:
    UnauthorizedException(Result result, AuthResult authResult);
};

} // namespace nx::network::rest
