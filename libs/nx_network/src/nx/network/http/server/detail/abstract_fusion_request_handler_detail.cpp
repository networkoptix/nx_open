#include "abstract_fusion_request_handler_detail.h"

namespace nx::network::http::detail {

void BaseFusionRequestHandler::requestCompleted(FusionRequestResult result)
{
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> outputMsgBody;

    if (nx::network::http::Method::isMessageBodyAllowedInResponse(
            m_requestMethod, result.httpStatusCode()))
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

bool BaseFusionRequestHandler::getInputFormat(
    const nx::network::http::Request& request,
    FusionRequestResult* errorDescription)
{
    m_inputFormat = Qn::SerializationFormat::UnsupportedFormat;
    QByteArray formatString;

    // Input/output formats can differ. E.g., GET request can specify input data in url query only.
    // TODO: #ak Fetching m_outputDataFormat from url query.
    if (!nx::network::http::Method::isMessageBodyAllowed(request.requestLine.method))
    {
        m_inputFormat = Qn::UrlQueryFormat;
    }
    else //< POST, PUT
    {
        const auto contentTypeIter = request.headers.find("Content-Type");
        if (contentTypeIter != request.headers.cend())
        {
            // TODO: #ak Add Content-Type header parser to nx::network::http.
            formatString = contentTypeIter->second.split(';')[0];
            m_inputFormat = Qn::serializationFormatFromHttpContentType(formatString);
        }
    }

    if (!isFormatSupported(m_inputFormat))
    {
        *errorDescription = FusionRequestResult(
            FusionRequestErrorClass::badRequest,
            QnLexical::serialized(FusionRequestErrorDetail::notAcceptable),
            FusionRequestErrorDetail::notAcceptable,
            lm("Input format %1 not supported").args(formatString));
        return false;
    }

    return true;
}

bool BaseFusionRequestHandler::getOutputFormat(
    const nx::network::http::Request& /*request*/,
    FusionRequestResult* /*errorDescription*/)
{
    m_outputFormat = Qn::SerializationFormat::JsonFormat;

    // TODO: #ak Fetching m_outputDataFormat from url query.
    // TODO: #ak Fetch format from Accept header.

#if 0
    if (*format == Qn::SerializationFormat::UnsupportedFormat)
    {
        const auto contentTypeIter = request.headers.find("Content-Type");
        if (contentTypeIter != request.headers.cend())
        {
            // Using same output format as input.
            const QByteArray dataFormatStr = contentTypeIter->second.split(';')[0];
            *format = Qn::serializationFormatFromHttpContentType(dataFormatStr);
        }
    }

    if (!isFormatSupported(*format))
    {
        *errorDescription = FusionRequestResult(
            FusionRequestErrorClass::badRequest,
            QnLexical::serialized(FusionRequestErrorDetail::notAcceptable),
            FusionRequestErrorDetail::notAcceptable,
            lm("Output format %1 not supported").args(
                Qn::serializationFormatToHttpContentType(m_outputDataFormat)));
        return false;
    }
#endif

    return true;
}

bool BaseFusionRequestHandler::isFormatSupported(
    Qn::SerializationFormat dataFormat) const
{
    return dataFormat == Qn::JsonFormat || dataFormat == Qn::UrlQueryFormat;
}

void BaseFusionRequestHandler::setConnectionEvents(
    nx::network::http::ConnectionEvents connectionEvents)
{
    m_connectionEvents = std::move(connectionEvents);
}

} // namespace nx::network::http::detail
