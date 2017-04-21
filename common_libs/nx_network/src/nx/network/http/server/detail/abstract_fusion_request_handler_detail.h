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

namespace nx_http {
namespace detail {

/**
 * This is a dummy implementation for types that
 * do not implement deserialization from url query.
 */
inline bool loadFromUrlQuery(...)
{
    return false;
}

class BaseFusionRequestHandler:
    public AbstractHttpRequestHandler
{
public:
    BaseFusionRequestHandler():
        m_outputDataFormat(Qn::JsonFormat)
    {
    }

protected:
    RequestProcessedHandler m_completionHandler;
    Qn::SerializationFormat m_outputDataFormat;
    nx_http::Method::ValueType m_requestMethod;

    void requestCompleted(FusionRequestResult result)
    {
        std::unique_ptr<nx_http::AbstractMsgBodySource> outputMsgBody;
        outputMsgBody = std::make_unique<nx_http::BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
            QJson::serialized(result));

        requestCompleted(
            result.httpStatusCode(),
            std::move(outputMsgBody));
    }

    /**
     * Call this method after request has been processed.
     */
    void requestCompleted(
        nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> outputMsgBody =
        std::unique_ptr<nx_http::AbstractMsgBodySource>())
    {
        auto completionHandler = std::move(m_completionHandler);
        nx_http::RequestResult requestResult(
            statusCode,
            std::move(outputMsgBody));
        if (m_connectionEvents)
            requestResult.connectionEvents = std::move(*m_connectionEvents);
        completionHandler(std::move(requestResult));
    }

    bool getDataFormat(
        const nx_http::Request& request,
        Qn::SerializationFormat* const inputDataFormat = nullptr)
    {
        // input/output formats can differ. E.g., GET request can specify input data in url query only.
        // TODO #ak: Fetching m_outputDataFormat from url query.
        //if( no format in query )
        //{
        if (request.requestLine.method == nx_http::Method::GET)
        {
            if (inputDataFormat)
            {
                // TODO #ak fetching format from Accept header.
                *inputDataFormat = Qn::UrlQueryFormat;
            }
        }
        else    //POST, PUT
        {
            const auto contentTypeIter = request.headers.find("Content-Type");
            if (contentTypeIter != request.headers.cend())
            {
                // TODO: #ak add Content-Type header parser to nx_http.
                const QByteArray dataFormatStr = contentTypeIter->second.split(';')[0];
                m_outputDataFormat = Qn::serializationFormatFromHttpContentType(dataFormatStr);
            }
            if (inputDataFormat)
                *inputDataFormat = m_outputDataFormat;
        }
        //}

        if (inputDataFormat && !isFormatSupported(*inputDataFormat))
        {
            FusionRequestResult result(
                FusionRequestErrorClass::badRequest,
                QnLexical::serialized(FusionRequestErrorDetail::notAcceptable),
                FusionRequestErrorDetail::notAcceptable,
                lm("Input format %1 not supported. Input data attributes "
                    "can be specified in url or by %2 POST message body")
                    .arg(Qn::serializationFormatToHttpContentType(*inputDataFormat))
                    .arg(Qn::serializationFormatToHttpContentType(Qn::JsonFormat)));
            requestCompleted(std::move(result));
            return false;
        }

        if (!isFormatSupported(m_outputDataFormat))
        {
            FusionRequestResult result(
                FusionRequestErrorClass::badRequest,
                QnLexical::serialized(FusionRequestErrorDetail::notAcceptable),
                FusionRequestErrorDetail::notAcceptable,
                lm("Output format %1 not supported. Only %2 is supported")
                    .arg(Qn::serializationFormatToHttpContentType(m_outputDataFormat))
                    .arg(Qn::serializationFormatToHttpContentType(Qn::JsonFormat)));
            requestCompleted(std::move(result));
            return false;
        }

        return true;
    }

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

    bool isFormatSupported(Qn::SerializationFormat dataFormat) const
    {
        return dataFormat == Qn::JsonFormat || dataFormat == Qn::UrlQueryFormat;
    }

    void setConnectionEvents(nx_http::ConnectionEvents connectionEvents)
    {
        m_connectionEvents = std::move(connectionEvents);
    }

private:
    boost::optional<nx_http::ConnectionEvents> m_connectionEvents;
};


template<typename Output>
class BaseFusionRequestHandlerWithOutput:
    public BaseFusionRequestHandler
{
public:
    /**
     * Call this method after request has been processed.
     * @note This overload is for requests returning something.
     */
    void requestCompleted(
        FusionRequestResult result,
        Output outputData = Output())
    {
        std::unique_ptr<nx_http::AbstractMsgBodySource> outputMsgBody;

        if (result.errorClass == FusionRequestErrorClass::noError)
        {
            // Serializing outputData.
            nx::Buffer serializedData;
            if (serializeToAnyFusionFormat(outputData, m_outputDataFormat, &serializedData))
            {
                outputMsgBody = std::make_unique<nx_http::BufferSource>(
                    Qn::serializationFormatToHttpContentType(m_outputDataFormat),
                    std::move(serializedData));
            }
            else
            {
                result.errorClass = FusionRequestErrorClass::internalError;
                result.resultCode = QnLexical::serialized(FusionRequestErrorDetail::responseSerializationError);
                result.errorDetail = static_cast<int>(FusionRequestErrorDetail::responseSerializationError);
            }
        }

        if (result.errorClass != FusionRequestErrorClass::noError)
        {
            outputMsgBody = std::make_unique<nx_http::BufferSource>(
                Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
                QJson::serialized(result));
        }

        BaseFusionRequestHandler::requestCompleted(
            result.httpStatusCode(),
            std::move(outputMsgBody));
    }
};


template<>
class BaseFusionRequestHandlerWithOutput<void>:
    public BaseFusionRequestHandler
{
public:
};

/**
 * @note for requests that set Input to non-void.
 */
template<typename Input, typename Output, typename Descendant>
class BaseFusionRequestHandlerWithInput:
    public BaseFusionRequestHandlerWithOutput<Output>
{
private:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        RequestProcessedHandler completionHandler) override
    {
        this->m_completionHandler = std::move(completionHandler);
        this->m_requestMethod = request.requestLine.method;

        Qn::SerializationFormat inputDataFormat = this->m_outputDataFormat;
        if (!this->getDataFormat(request, &inputDataFormat))
            return;

        // Parsing request message body using fusion.
        Input inputData;
        if (!this->template parseAnyFusionFormat<Input>(
                inputDataFormat,
                request.requestLine.method == nx_http::Method::GET
                    ? request.requestLine.url.query().toUtf8()
                    : request.messageBody,
                &inputData))
        {
            FusionRequestResult result(
                FusionRequestErrorClass::badRequest,
                QnLexical::serialized(FusionRequestErrorDetail::deserializationError),
                FusionRequestErrorDetail::deserializationError,
                lit("Error deserializing input of type %1").
                arg(Qn::serializationFormatToHttpContentType(inputDataFormat)));
            this->requestCompleted(std::move(result));
            return;
        }

        static_cast<Descendant*>(this)->processRequest(
            connection,
            request,
            std::move(authInfo),
            std::move(inputData));
    }
};

} // namespace detail
} // namespace nx_http
