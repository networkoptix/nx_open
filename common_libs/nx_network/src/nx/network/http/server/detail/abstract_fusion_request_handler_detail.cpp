#include "abstract_fusion_request_handler_detail.h"

namespace nx {
namespace network {
namespace http {
namespace detail {

BaseFusionRequestHandler::BaseFusionRequestHandler():
    m_outputDataFormat(Qn::JsonFormat)
{
}

void BaseFusionRequestHandler::requestCompleted(FusionRequestResult result)
{
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody;

    if (nx::network::http::StatusCode::isMessageBodyAllowed(result.httpStatusCode()))
    {
        outputMsgBody = std::make_unique<nx::network::http::BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
            QJson::serialized(result));
    }

    requestCompleted(
        result.httpStatusCode(),
        std::move(outputMsgBody));
}

void BaseFusionRequestHandler::requestCompleted(
    nx::network::http::StatusCode::Value statusCode,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody)
{
    auto completionHandler = std::move(m_completionHandler);
    nx::network::http::RequestResult requestResult(
        statusCode,
        std::move(outputMsgBody));
    if (m_connectionEvents)
        requestResult.connectionEvents = std::move(*m_connectionEvents);
    completionHandler(std::move(requestResult));
}

bool BaseFusionRequestHandler::getDataFormat(
    const nx::network::http::Request& request,
    Qn::SerializationFormat* inputDataFormat,
    FusionRequestResult* errorDescription)
{
    // Input/output formats can differ. E.g., GET request can specify input data in url query only.
    // TODO: #ak Fetching m_outputDataFormat from url query.
    if (!nx::network::http::Method::isMessageBodyAllowed(request.requestLine.method))
    {
        if (inputDataFormat)
        {
            // TODO: #ak Fetching format from Accept header.
            *inputDataFormat = Qn::UrlQueryFormat;
        }
    }
    else //< POST, PUT
    {
        const auto contentTypeIter = request.headers.find("Content-Type");
        if (contentTypeIter != request.headers.cend())
        {
            // TODO: #ak Add Content-Type header parser to nx_http.
            const QByteArray dataFormatStr = contentTypeIter->second.split(';')[0];
            m_outputDataFormat = Qn::serializationFormatFromHttpContentType(dataFormatStr);
        }
        if (inputDataFormat)
            *inputDataFormat = m_outputDataFormat;
    }

    if (inputDataFormat && !isFormatSupported(*inputDataFormat))
    {
        *errorDescription = FusionRequestResult(
            FusionRequestErrorClass::badRequest,
            QnLexical::serialized(FusionRequestErrorDetail::notAcceptable),
            FusionRequestErrorDetail::notAcceptable,
            lm("Input format %1 not supported. Input data attributes "
                "can be specified in url or by %2 POST message body")
                .arg(Qn::serializationFormatToHttpContentType(*inputDataFormat))
                .arg(Qn::serializationFormatToHttpContentType(Qn::JsonFormat)));
        return false;
    }

    if (!isFormatSupported(m_outputDataFormat))
    {
        *errorDescription = FusionRequestResult(
            FusionRequestErrorClass::badRequest,
            QnLexical::serialized(FusionRequestErrorDetail::notAcceptable),
            FusionRequestErrorDetail::notAcceptable,
            lm("Output format %1 not supported. Only %2 is supported")
                .arg(Qn::serializationFormatToHttpContentType(m_outputDataFormat))
                .arg(Qn::serializationFormatToHttpContentType(Qn::JsonFormat)));
        return false;
    }

    return true;
}

bool BaseFusionRequestHandler::isFormatSupported(Qn::SerializationFormat dataFormat) const
{
    return dataFormat == Qn::JsonFormat || dataFormat == Qn::UrlQueryFormat;
}

void BaseFusionRequestHandler::setConnectionEvents(nx::network::http::ConnectionEvents connectionEvents)
{
    m_connectionEvents = std::move(connectionEvents);
}

} // namespace detail
} // namespace nx
} // namespace network
} // namespace http
