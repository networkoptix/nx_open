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

namespace nx {
namespace network {
namespace http {
namespace detail {

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
    BaseFusionRequestHandler();

protected:
    RequestProcessedHandler m_completionHandler;
    Qn::SerializationFormat m_outputDataFormat;
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

    bool getDataFormat(
        const nx::network::http::Request& request,
        Qn::SerializationFormat* inputDataFormat,
        FusionRequestResult* errorDescription);

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

private:
    boost::optional<nx::network::http::ConnectionEvents> m_connectionEvents;

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        processRawHttpRequest(
            std::move(requestContext),
            std::move(completionHandler));
    }

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
        if (!serializeToAnyFusionFormat(output, m_outputDataFormat, &serializedData))
            return false;

        *outputMsgBody = std::make_unique<nx::network::http::BufferSource>(
            Qn::serializationFormatToHttpContentType(m_outputDataFormat),
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

        Qn::SerializationFormat inputDataFormat = this->m_outputDataFormat;
        FusionRequestResult errorDescription;
        if (!this->getDataFormat(request, &inputDataFormat, &errorDescription))
        {
            this->requestCompleted(std::move(errorDescription));
            return;
        }

        // Parsing request message body using fusion.
        Input inputData;
        if (!this->template parseAnyFusionFormat<Input>(
                inputDataFormat,
                request.requestLine.method == nx::network::http::Method::get
                    ? request.requestLine.url.query().toUtf8()
                    : request.messageBody,
                &inputData))
        {
            FusionRequestResult result(
                FusionRequestErrorClass::badRequest,
                QnLexical::serialized(FusionRequestErrorDetail::deserializationError),
                FusionRequestErrorDetail::deserializationError,
                QStringLiteral("Error deserializing input of type %1").
                    arg(Qn::serializationFormatToHttpContentType(inputDataFormat)));
            this->requestCompleted(std::move(result));
            return;
        }

        static_cast<Descendant*>(this)->processRequest(
            std::move(requestContext),
            std::move(inputData));
    }
};

} // namespace detail
} // namespace nx
} // namespace network
} // namespace http
