#pragma once

#include "../abstract_http_request_handler.h"

#include <type_traits>

#include <boost/optional.hpp>

#include <QtCore/QUrlQuery>

#include <nx/fusion/serialization_format.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/std/cpp14.h>

#include "../fusion_request_result.h"
#include "../../buffer_source.h"

namespace nx::network::http::detail {

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

class NX_NETWORK_API BaseFusionRequestHandler:
    public AbstractHttpRequestHandler
{
    using base_type = AbstractHttpRequestHandler;

public:
    BaseFusionRequestHandler() = default;

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        processRawHttpRequest(
            std::move(requestContext),
            std::move(completionHandler));
    }

protected:
    RequestProcessedHandler m_completionHandler;
    nx::network::http::Method::ValueType m_requestMethod;

    virtual void processRawHttpRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) = 0;

    virtual void sendRawHttpResponse(RequestResult requestResult)
    {
        base_type::sendResponse(std::move(requestResult));
    }

    void requestCompleted(FusionRequestResult result);

    /**
     * Call this method after request has been processed.
     */
    void requestCompleted(
        nx::network::http::StatusCode::Value statusCode,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody = nullptr);

    template<class T>
    bool parseAnyFusionFormat(
        Qn::SerializationFormat dataFormat,
        const nx::Buffer& data,
        T* const target)
    {
        bool success = false;
        switch (dataFormat)
        {
            case Qn::UrlQueryFormat:
                return loadFromUrlQuery(
                    QUrlQuery(QUrl::fromPercentEncoding(data)),
                    target);

            case Qn::JsonFormat:
                *target = QJson::deserialized<T>(data, T(), &success);
                return success;

                //case Qn::UbjsonFormat:
                //    *target = QnUbjson::deserialized<T>( data, T(), &success );
                //    return success;

            default:
                return false;
        }
    }

    template<class T>
    bool serializeToAnyFusionFormat(
        const T& data,
        Qn::SerializationFormat dataFormat,
        nx::Buffer* const output)
    {
        switch (dataFormat)
        {
            case Qn::UrlQueryFormat:
                NX_ASSERT(false);
                return true;

            case Qn::JsonFormat:
                *output = QJson::serialized(data);
                return true;

            //case Qn::UbjsonFormat:
            //    *output = QnUbjson::serialized<T>( data );
            //    return true;

            default:
                return false;
        }
    }

    bool isFormatSupported(Qn::SerializationFormat dataFormat) const;
    void setConnectionEvents(nx::network::http::ConnectionEvents connectionEvents);

    bool getInputFormat(
        const nx::network::http::Request& request,
        FusionRequestResult* errorDescription);

    Qn::SerializationFormat inputFormat() const { return m_inputFormat; }

    bool getOutputFormat(
        const nx::network::http::Request& request,
        FusionRequestResult* errorDescription);

    Qn::SerializationFormat outputFormat() const { return m_outputFormat; }

private:
    boost::optional<nx::network::http::ConnectionEvents> m_connectionEvents;
    Qn::SerializationFormat m_inputFormat = Qn::UnsupportedFormat;
    Qn::SerializationFormat m_outputFormat = Qn::UnsupportedFormat;

    virtual void sendResponse(RequestResult requestResult) override
    {
        sendRawHttpResponse(std::move(requestResult));
    }
};


template<typename Output>
class BaseFusionRequestHandlerWithOutput:
    public BaseFusionRequestHandler
{
public:
    /**
     * Call this method after request has been processed.
     * NOTE: This overload is for requests returning something.
     */
    void requestCompleted(
        FusionRequestResult result,
        Output outputData = Output())
    {
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody;

        if (result.errorClass == FusionRequestErrorClass::noError)
        {
            if (!serializeOutputToResponse(result, outputData, &outputMsgBody))
            {
                result.errorClass = FusionRequestErrorClass::internalError;
                result.resultCode = QnLexical::serialized(FusionRequestErrorDetail::responseSerializationError);
                result.errorDetail = static_cast<int>(FusionRequestErrorDetail::responseSerializationError);
            }
        }

        if (result.errorClass != FusionRequestErrorClass::noError &&
            nx::network::http::Method::isMessageBodyAllowedInResponse(
                m_requestMethod, result.httpStatusCode()))
        {
            outputMsgBody = std::make_unique<nx::network::http::BufferSource>(
                Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
                QJson::serialized(result));
        }

        BaseFusionRequestHandler::requestCompleted(
            result.httpStatusCode(),
            std::move(outputMsgBody));
    }

private:
    bool serializeOutputToResponse(
        const FusionRequestResult& result,
        const Output& output,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource>* outputMsgBody)
    {
        if (nx::network::http::Method::isMessageBodyAllowedInResponse(
                m_requestMethod, result.httpStatusCode()))
        {
            return serializeOutputAsMessageBody(output, outputMsgBody);
        }
        else
        {
            return serializeToHeaders(&response()->headers, output);
        }
    }

    bool serializeOutputAsMessageBody(
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
};

template<>
class BaseFusionRequestHandlerWithOutput<void>:
    public BaseFusionRequestHandler
{
public:
};

/**
 * NOTE: For requests that set Input to non-void.
 */
template<typename Input, typename Output, typename Descendant>
class BaseFusionRequestHandlerWithInput:
    public BaseFusionRequestHandlerWithOutput<Output>
{
protected:
    virtual void processRawHttpRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) override
    {
        const auto& request = requestContext.request;

        this->m_completionHandler = std::move(completionHandler);
        this->m_requestMethod = request.requestLine.method;

        if constexpr (!std::is_same_v<Output, void>)
        {
            FusionRequestResult error;
            if (!this->getOutputFormat(request, &error))
            {
                this->requestCompleted(std::move(error));
                return;
            }
        }

        if (!requestMethodCorrespondsToTheHanderSpecialization())
        {
            FusionRequestResult error;
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
            FusionRequestResult error;
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

private:
    bool requestMethodCorrespondsToTheHanderSpecialization() const
    {
        // Forbidding ignoring request body.
        // So, Input=void cannot be used with a method that may contain a request body.
        // TODO: #ak Currently, these classes do not distinguish request body and query.
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

    template<typename LocalInput>
    bool parseInput(
        const Request& request,
        LocalInput* input,
        FusionRequestResult* error,
        typename std::enable_if<!std::is_same_v<LocalInput, void>>::type* = nullptr)
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
            *error = FusionRequestResult(
                FusionRequestErrorClass::badRequest,
                QnLexical::serialized(FusionRequestErrorDetail::deserializationError),
                FusionRequestErrorDetail::deserializationError,
                QStringLiteral("Error deserializing input of type %1").
                    arg(Qn::serializationFormatToHttpContentType(this->inputFormat())));
            return false;
        }

        return true;
    }
};

} // namespace nx::network::http::detail
