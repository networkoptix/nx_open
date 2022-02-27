// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/instrument.h>

#include "../http_types.h"

namespace nx::network::http {

NX_REFLECTION_ENUM_CLASS(ApiRequestErrorClass,
    noError = 0,
    badRequest,
    unauthorized,
    logicError,
    notFound,
    ioError,
    //!indicates server bug?
    internalError
)

NX_REFLECTION_ENUM_CLASS(ApiRequestErrorDetail,
    ok = 0,
    responseSerializationError,
    deserializationError,
    notAcceptable
)

static constexpr char kErrorClass[] = "errorClass";
static constexpr char kResultCode[] = "resultCode";
static constexpr char kErrorDetail[] = "errorDetail";
static constexpr char kErrorText[] = "errorText";

class ApiRequestResult : public std::map<std::string, std::string>
{
public:
    ApiRequestErrorClass getErrorClass() const;
    const std::string& getResultCode() const;
    int getErrorDetail() const;
    const std::string& getErrorText() const;

    void setErrorClass(ApiRequestErrorClass errorClass);
    void setResultCode(const std::string& code);
    void setErrorDetail(int num);
    void setErrorText(const std::string& text);

    /** Creates OK result */
    ApiRequestResult();

    ApiRequestResult(ApiRequestErrorClass errorClass);

    ApiRequestResult(
        ApiRequestErrorClass _errorClass,
        const std::string& _resultCode,
        int _errorDetail,
        const std::string& _errorText);

    ApiRequestResult(
        ApiRequestErrorClass _errorClass,
        const std::string& _resultCode,
        ApiRequestErrorDetail _errorDetail,
        const std::string& _errorText);

    ApiRequestResult(StatusCode::Value statusCode);

    nx::network::http::StatusCode::Value httpStatusCode() const;
    void setHttpStatusCode(nx::network::http::StatusCode::Value statusCode);

private:
    std::optional<nx::network::http::StatusCode::Value> m_httpStatusCode;

    nx::network::http::StatusCode::Value calculateHttpStatusCode() const;
};

NX_NETWORK_API ApiRequestErrorClass fromHttpStatus(StatusCode::Value statusCode);

// The class defined in .h file because exporting class inherited from std container cause linker
// errors in VS
inline ApiRequestResult::ApiRequestResult()
{
    emplace(kErrorClass, nx::reflect::toString(ApiRequestErrorClass::noError));
    emplace(kResultCode, nx::reflect::toString(ApiRequestErrorDetail::ok));
    emplace(kErrorDetail, std::to_string(static_cast<int>(ApiRequestErrorDetail::ok)));
    emplace(kErrorText, "");
}

inline ApiRequestResult::ApiRequestResult(ApiRequestErrorClass errorClass): ApiRequestResult()
{
    at(kErrorClass) = nx::reflect::toString(errorClass);
}

inline ApiRequestResult::ApiRequestResult(StatusCode::Value statusCode): ApiRequestResult()
{
    m_httpStatusCode = statusCode;
}

inline ApiRequestResult::ApiRequestResult(
    ApiRequestErrorClass _errorClass,
    const std::string& _resultCode,
    int _errorDetail,
    const std::string& _errorText)
{
    emplace(kErrorClass, nx::reflect::toString(_errorClass));
    emplace(kResultCode, _resultCode);
    emplace(kErrorDetail, std::to_string(_errorDetail));
    emplace(kErrorText, _errorText);
}

inline ApiRequestResult::ApiRequestResult(
    ApiRequestErrorClass _errorClass,
    const std::string& _resultCode,
    ApiRequestErrorDetail _errorDetail,
    const std::string& _errorText):
    ApiRequestResult(_errorClass, _resultCode, static_cast<int>(_errorDetail), _errorText)
{
}

inline void ApiRequestResult::setHttpStatusCode(nx::network::http::StatusCode::Value statusCode)
{
    m_httpStatusCode = statusCode;
}

inline nx::network::http::StatusCode::Value ApiRequestResult::httpStatusCode() const
{
    if (m_httpStatusCode)
        return *m_httpStatusCode;
    else
        return calculateHttpStatusCode();
}

inline nx::network::http::StatusCode::Value ApiRequestResult::calculateHttpStatusCode() const
{
    ApiRequestErrorClass errorClassVal;
    nx::reflect::fromString(at(kErrorClass), &errorClassVal);
    switch (errorClassVal)
    {
        case ApiRequestErrorClass::noError:
            return nx::network::http::StatusCode::ok;

        case ApiRequestErrorClass::badRequest:
            switch (static_cast<ApiRequestErrorDetail>(std::stoi(at(kErrorDetail))))
            {
                case ApiRequestErrorDetail::notAcceptable:
                    return nx::network::http::StatusCode::notAcceptable;
                default:
                    return nx::network::http::StatusCode::badRequest;
            }

        case ApiRequestErrorClass::unauthorized:
            // This is authorization failure, not authentication!
            // "401 Unauthorized" is not applicable here since it
            // actually signals authentication error.
            return nx::network::http::StatusCode::forbidden;

        case ApiRequestErrorClass::logicError:
        case ApiRequestErrorClass::notFound:
            // Using "404 Not Found" to signal any logic error.
            // It is allowed by HTTP. See [rfc2616, 10.4.5] for details
            return nx::network::http::StatusCode::notFound;

        case ApiRequestErrorClass::ioError:
            return nx::network::http::StatusCode::serviceUnavailable;

        case ApiRequestErrorClass::internalError:
            return nx::network::http::StatusCode::internalServerError;

        default:
            NX_ASSERT(false);
            return nx::network::http::StatusCode::internalServerError;
    }
}

inline ApiRequestErrorClass ApiRequestResult::getErrorClass() const
{
    ApiRequestErrorClass errorClassVal;
    nx::reflect::fromString(at(kErrorClass), &errorClassVal);
    return errorClassVal;
}

inline const std::string& ApiRequestResult::getResultCode() const
{
    return at(kResultCode);
}

inline int ApiRequestResult::getErrorDetail() const
{
    return std::stoi(at(kErrorDetail));
}

inline const std::string& ApiRequestResult::getErrorText() const
{
    return at(kErrorText);
}

inline void ApiRequestResult::setErrorClass(ApiRequestErrorClass errorClass)
{
    this->operator[](kErrorClass) = nx::reflect::toString(errorClass);
}

inline void ApiRequestResult::setResultCode(const std::string& code)
{
    this->operator[](kResultCode) = code;
}

inline void ApiRequestResult::setErrorDetail(int num)
{
    this->operator[](kErrorDetail) = std::to_string(num);
}

inline void ApiRequestResult::setErrorText(const std::string& text)
{
    this->operator[](kErrorText) = text;
}

QN_FUSION_DECLARE_FUNCTIONS(nx::network::http::ApiRequestErrorDetail, (lexical), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::network::http::ApiRequestErrorClass, (lexical), NX_NETWORK_API)

} // namespace nx::network::http
