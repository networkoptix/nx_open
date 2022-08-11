// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <type_traits>

#include <QtCore/QUrlQuery>

#include <nx/fusion/fusion/fusion_adaptors.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization_format.h>
#include <nx/reflect/json.h>
#include <nx/reflect/urlencoded.h>
#include <nx/utils/serialization/qt_geometry_reflect_json.h>
#include <nx/utils/std/cpp14.h>

#include "../../buffer_source.h"
#include "../abstract_http_request_handler.h"
#include "../api_request_result.h"

namespace nx::network::http::detail {

class NxReflectBinder
{
public:
    template<typename T>
    static std::tuple<nx::Buffer, bool /*result*/> serialized(
        Qn::SerializationFormat format, const T& data)
    {
        switch (format)
        {
            case Qn::JsonFormat:
                return std::make_tuple(
                    nx::Buffer(nx::reflect::json::serialize(data)), true);

            case Qn::UrlEncodedFormat:
                return std::make_tuple(nx::Buffer(nx::reflect::urlencoded::serialize(data)), true);

            default:
                return std::make_tuple(nx::Buffer(), false);
        }
    }

    template<class T>
    static bool deserialize(
        Qn::SerializationFormat format,
        const nx::Buffer& data,
        T* const target)
    {
        switch (format)
        {
            case Qn::JsonFormat:
                return nx::reflect::json::deserialize(
                    std::string_view(data.data(), data.size()), target);

            case Qn::UrlEncodedFormat:
                return nx::reflect::urlencoded::deserialize(
                    std::string_view(data.data(), data.size()), target);

            default:
                return false;
        }
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * This is a dummy implementation for types that
 * do not implement deserialization from url query.
 */
inline bool loadFromUrlQuery(const QUrlQuery&, ...)
{
    return false;
}

template<typename T>
bool serializeToHeaders(nx::network::http::HttpHeaders* /*where*/, const T& /*what*/)
{
    NX_ASSERT(false);
    return false;
}

template<typename Input, typename Descendant>
class BaseApiRequestHandler:
    public RequestHandlerWithContext
{
    using base_type = RequestHandlerWithContext;
    using FormatBinder = NxReflectBinder;

public:
    BaseApiRequestHandler() = default;

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

protected:
    RequestProcessedHandler m_completionHandler;
    nx::network::http::Method m_requestMethod;

    /**
     * Call this method after request has been processed.
     * NOTE: This overload is for requests returning something.
     */
    template<typename... Output>
    // requires sizeof...(Output) <= 1
    void requestCompleted(
        ApiRequestResult result,
        Output... output);

    void requestCompleted(
        nx::network::http::StatusCode::Value statusCode,
        nx::network::http::HttpHeaders responseHeaders,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody);

    template<class T>
    bool parseAnyFusionFormat(
        Qn::SerializationFormat dataFormat,
        const nx::Buffer& data,
        T* const target);

    template<class T>
    bool serializeToAnyFusionFormat(
        const T& data,
        Qn::SerializationFormat dataFormat,
        nx::Buffer* const output);

    bool isFormatSupported(Qn::SerializationFormat dataFormat) const;
    void setConnectionEvents(nx::network::http::ConnectionEvents connectionEvents);

    bool getInputFormat(
        const nx::network::http::Request& request,
        ApiRequestResult* errorDescription);

    Qn::SerializationFormat inputFormat() const { return m_inputFormat; }

    bool getOutputFormat(
        const nx::network::http::Request& request,
        ApiRequestResult* errorDescription);

    Qn::SerializationFormat outputFormat() const { return m_outputFormat; }

private:
    template <typename Output>
    bool serializeOutputToResponse(
        const ApiRequestResult& result,
        const Output& output,
        HttpHeaders* responseHeaders,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource>* outputMsgBody);

    template <typename Output>
    bool serializeOutputAsMessageBody(
        const Output& output,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource>* outputMsgBody);

    bool requestMethodCorrespondsToTheHanderSpecialization() const;

    template<typename LocalInput>
    bool parseInput(
        const Request& request,
        LocalInput* input,
        ApiRequestResult* error,
        typename std::enable_if<!std::is_same_v<LocalInput, void>>::type* = nullptr);

private:
    std::optional<nx::network::http::ConnectionEvents> m_connectionEvents;
    Qn::SerializationFormat m_inputFormat = Qn::UnsupportedFormat;
    Qn::SerializationFormat m_outputFormat = Qn::UnsupportedFormat;
};

//-------------------------------------------------------------------------------------------------

template<typename Input, typename Descendant>
void BaseApiRequestHandler<Input, Descendant>::processRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;

    this->m_completionHandler = std::move(completionHandler);
    this->m_requestMethod = request.requestLine.method;

    // TODO: #akolesnikov Not good that request that does not provide any result may be rejected
    // with "unsupported format" error.
    ApiRequestResult error;
    if (!this->getOutputFormat(request, &error))
    {
        this->requestCompleted(std::move(error));
        return;
    }

    if (!requestMethodCorrespondsToTheHanderSpecialization())
    {
        ApiRequestResult error;
        error.setHttpStatusCode(StatusCode::notAllowed);
        this->requestCompleted(std::move(error));
        return;
    }

    if constexpr (std::is_same_v<Input, void>)
    {
        // No input.
        static_cast<Descendant*>(this)->processRequest(std::move(requestContext));
    }
    else
    {
        Input input;
        ApiRequestResult error;
        if (!parseInput(request, &input, &error))
        {
            this->requestCompleted(std::move(error));
            return;
        }

        static_cast<Descendant*>(this)->processRequest(
            std::move(requestContext),
            std::move(input));
    }
}

template<typename Input, typename Descendant>
template<typename... Output>
// requires sizeof...(Output) <= 1
void BaseApiRequestHandler<Input, Descendant>::requestCompleted(
    ApiRequestResult result,
    Output... output)
{
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody;

    HttpHeaders responseHeaders;
    if constexpr (sizeof...(output) > 1)
        responseHeaders = std::get<1>(std::forward_as_tuple(output...));

    if constexpr (sizeof...(output) > 0)
    {
        if (result.getErrorClass() == ApiRequestErrorClass::noError)
        {
            if (!serializeOutputToResponse(
                    result,
                    std::get<0>(std::forward_as_tuple(output...)),
                    &responseHeaders,
                    &outputMsgBody))
            {
                result.setErrorClass(ApiRequestErrorClass::internalError);
                result.setResultCode(nx::reflect::toString(ApiRequestErrorDetail::responseSerializationError));
                result.setErrorDetail(static_cast<int>(ApiRequestErrorDetail::responseSerializationError));
            }
        }
    }

    bool reportResultInBody = result.getErrorClass() != ApiRequestErrorClass::noError;

    // TODO: #akolesnikov This logic is for reverse compatibility.
    // It should be moved closer to specific business logic.
    if constexpr (sizeof...(output) == 0)
        reportResultInBody = true; //< Reporting even success result as body.

    if (reportResultInBody &&
        nx::network::http::Method::isMessageBodyAllowedInResponse(
            m_requestMethod, result.httpStatusCode()))
    {
        nx::Buffer serializedResult;
        NX_ASSERT(serializeToAnyFusionFormat(result, Qn::JsonFormat, &serializedResult));

        outputMsgBody = std::make_unique<nx::network::http::BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
            serializedResult);
    }

    requestCompleted(result.httpStatusCode(), std::move(responseHeaders), std::move(outputMsgBody));
}

template<typename Input, typename Descendant>
void BaseApiRequestHandler<Input, Descendant>::requestCompleted(
    nx::network::http::StatusCode::Value statusCode,
    nx::network::http::HttpHeaders responseHeaders,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody)
{
    auto completionHandler = std::move(m_completionHandler);

    nx::network::http::RequestResult requestResult(statusCode, std::move(outputMsgBody));
    requestResult.headers.merge(std::move(responseHeaders));
    if (m_connectionEvents)
        requestResult.connectionEvents = std::move(*m_connectionEvents);
    completionHandler(std::move(requestResult));
}

template<typename Input, typename Descendant>
template<class T>
bool BaseApiRequestHandler<Input, Descendant>::parseAnyFusionFormat(
    Qn::SerializationFormat dataFormat,
    const nx::Buffer& data,
    T* const target)
{
    switch (dataFormat)
    {
        case Qn::UrlQueryFormat:
            return loadFromUrlQuery(
                QUrlQuery(QUrl::fromPercentEncoding(data.toRawByteArray())),
                target);

        default:
            return FormatBinder::deserialize(dataFormat, data, target);
    }
}

template<typename Input, typename Descendant>
template<class T>
bool BaseApiRequestHandler<Input, Descendant>::serializeToAnyFusionFormat(
    const T& data,
    Qn::SerializationFormat dataFormat,
    nx::Buffer* const output)
{
    switch (dataFormat)
    {
        case Qn::UrlQueryFormat:
        {
            NX_ASSERT(false);
            return true;
        }

        default:
        {
            bool success = true;
            std::tie(*output, success) = FormatBinder::serialized(dataFormat, data);
            return success;
        }
    }
}

template<typename Input, typename Descendant>
bool BaseApiRequestHandler<Input, Descendant>::getInputFormat(
    const nx::network::http::Request& request,
    ApiRequestResult* errorDescription)
{
    m_inputFormat = Qn::SerializationFormat::UnsupportedFormat;
    std::string formatString;

    // Input/output formats can differ. E.g., GET request can specify input data in url query only.
    // TODO: #akolesnikov Fetching m_outputDataFormat from url query.
    if (!nx::network::http::Method::isMessageBodyAllowed(request.requestLine.method))
    {
        m_inputFormat = Qn::UrlQueryFormat;
    }
    else //< POST, PUT
    {
        const auto contentTypeIter = request.headers.find("Content-Type");
        if (contentTypeIter != request.headers.cend())
        {
            formatString = header::ContentType(contentTypeIter->second).value;
            m_inputFormat = Qn::serializationFormatFromHttpContentType(formatString);
        }
    }

    if (!isFormatSupported(m_inputFormat))
    {
        *errorDescription = ApiRequestResult(
            ApiRequestErrorClass::badRequest,
            nx::reflect::toString(ApiRequestErrorDetail::notAcceptable),
            ApiRequestErrorDetail::notAcceptable,
            nx::format("Input format %1 not supported", formatString).toStdString());
        return false;
    }

    return true;
}

template<typename Input, typename Descendant>
bool BaseApiRequestHandler<Input, Descendant>::getOutputFormat(
    const nx::network::http::Request& request,
    ApiRequestResult* errorDescription)
{
    m_outputFormat = Qn::SerializationFormat::JsonFormat;

    const QUrlQuery urlQuery(request.requestLine.url.query());
    const auto formatStr = urlQuery.queryItemValue("format");
    if (!formatStr.isEmpty())
    {
        bool parsed = false;
        const auto format =
            nx::reflect::fromString(formatStr.toStdString(), Qn::JsonFormat, &parsed);
        if (!parsed || !isFormatSupported(format))
        {
            *errorDescription = ApiRequestResult(
                ApiRequestErrorClass::badRequest,
                nx::reflect::toString(ApiRequestErrorDetail::notAcceptable),
                ApiRequestErrorDetail::notAcceptable,
                nx::format("Output format %1 not supported", formatStr).toStdString());
            return false;
        }

        m_outputFormat = format;
    }

    // TODO: #akolesnikov Fetch format from Accept header.

    return true;
}

template<typename Input, typename Descendant>
bool BaseApiRequestHandler<Input, Descendant>::isFormatSupported(
    Qn::SerializationFormat dataFormat) const
{
    return dataFormat == Qn::JsonFormat || dataFormat == Qn::UrlQueryFormat
        || dataFormat == Qn::UrlEncodedFormat;
}

template<typename Input, typename Descendant>
void BaseApiRequestHandler<Input, Descendant>::setConnectionEvents(
    nx::network::http::ConnectionEvents connectionEvents)
{
    m_connectionEvents = std::move(connectionEvents);
}

template<typename Input, typename Descendant>
template<typename Output>
bool BaseApiRequestHandler<Input, Descendant>::serializeOutputToResponse(
    const ApiRequestResult& result,
    const Output& output,
    HttpHeaders* responseHeaders,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource>* outputMsgBody)
{
    if (nx::network::http::Method::isMessageBodyAllowedInResponse(
            m_requestMethod, result.httpStatusCode()))
    {
        return serializeOutputAsMessageBody(output, outputMsgBody);
    }
    else
    {
        return serializeToHeaders(responseHeaders, output);
    }
}

template<typename Input, typename Descendant>
template<typename Output>
bool BaseApiRequestHandler<Input, Descendant>::serializeOutputAsMessageBody(
    const Output& output,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource>* outputMsgBody)
{
    nx::Buffer serializedData;
    if (!serializeToAnyFusionFormat(output, outputFormat(), &serializedData))
        return false;

    *outputMsgBody = std::make_unique<nx::network::http::BufferSource>(
        Qn::serializationFormatToHttpContentType(outputFormat()),
        std::move(serializedData));
    return true;
}

template<typename Input, typename Descendant>
bool BaseApiRequestHandler<Input, Descendant>::
    requestMethodCorrespondsToTheHanderSpecialization() const
{
    // Forbidding ignoring request body.
    // So, Input=void cannot be used with a method that may contain a request body.
    // TODO: #akolesnikov Currently, these classes do not distinguish request body and query.
    // So, making the check softer than it should be to take this into account.
    //constexpr bool isRequestMsgBodyExpected = !std::is_same_v<Input, void>;
    //if (!isRequestMsgBodyExpected && Method::isMessageBodyAllowed(this->m_requestMethod))
    //    return false;

    // For the response introducing a soft check: we may send an empty response body.
    // So, Output=void is allowed to be used with a method that may produce a response body.
    //constexpr bool isResponseMsgBodySuggested = !std::is_same_v<Output, void>;
    //if (isResponseMsgBodySuggested &&
    //    !Method::isMessageBodyAllowedInResponse(this->m_requestMethod, StatusCode::ok))
    //{
    //    return false;
    //}

    return true;
}

template<typename Input, typename Descendant>
template<typename LocalInput>
bool BaseApiRequestHandler<Input, Descendant>::parseInput(
    const Request& request,
    LocalInput* input,
    ApiRequestResult* error,
    typename std::enable_if<!std::is_same_v<LocalInput, void>>::type*)
{
    if (!this->getInputFormat(request, error))
        return false;

    // Parsing request message body using fusion.
    if (!this->template parseAnyFusionFormat<LocalInput>(
            this->inputFormat(),
            request.requestLine.method == nx::network::http::Method::get
                ? request.requestLine.url.query().toUtf8()
                : request.messageBody,
            input))
    {
        *error = ApiRequestResult(
            ApiRequestErrorClass::badRequest,
            nx::reflect::toString(ApiRequestErrorDetail::deserializationError),
            ApiRequestErrorDetail::deserializationError,
            nx::format("Error deserializing input of type %1",
                Qn::serializationFormatToHttpContentType(this->inputFormat())).toStdString());
        return false;
    }

    return true;
}

} // namespace nx::network::http::detail
