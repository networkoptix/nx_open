#include "result.h"

#include <nx/utils/switch.h>

namespace nx::network::rest {

QN_FUSION_DEFINE_FUNCTIONS(Result::Error, (numeric))

nx::network::http::StatusCode::Value Result::toHttpStatus(Error code)
{
    using namespace nx::network;
    switch (code)
    {
        case Result::Error::NoError:
            return http::StatusCode::ok;

        case Result::Error::InvalidParameter:
        case Result::Error::MissingParameter:
        case Result::Error::CantProcessRequest:
            return http::StatusCode::unprocessableEntity;

        case Result::Error::Forbidden:
            return http::StatusCode::forbidden;

        default:
            return http::StatusCode::badRequest;
    }
}

QString Result::ErrorDescriptor::text() const
{
    const auto message =
        [this](nx::utils::log::Message format, int argsToUse)
        {
            NX_ASSERT(m_arguments.size() >= argsToUse, containerString(m_arguments));
            const auto message = nx::utils::switch_(argsToUse,
                0, [&](){ return format; },
                1, [&](){ return format.arg(m_arguments.value(0)); },
                2, [&](){ return format.args(m_arguments.value(0), m_arguments.value(1)); },
                nx::utils::default_,
                [&]()
                {
                    NX_ASSERT(false, containerString(m_arguments));
                    argsToUse = 0;
                    return format;
                });

            if (m_arguments.size() <= argsToUse)
                return message;

            return lm("%1 - %2").args(message, containerString(m_arguments.mid(argsToUse)));
        };

    switch (m_code)
    {
        case NoError: return message("OK", 0);
        case MissingParameter: return message("Missing required parameter '%1'", 1);
        case InvalidParameter: return message("Invalid parameter '%1' value: '%2'", 2);
        case CantProcessRequest: return message("Failed to process request '%1'", 1);
        case Forbidden: return message("Forbidden", 0);
        case BadRequest: return message("Bad request", 0);
        case InternalServerError: return message("Internal server error", 0);
    }

    const auto error = lm("Unknown error code '%1'. Arguments: %2")
        .args(static_cast<int>(m_code), containerString(m_arguments));

    NX_ASSERT(false, error);
    return error;
}

void Result::setError(Error error_, QString errorString_)
{
    error = error_;
    errorString = std::move(errorString_);
}

void Result::setError(const Result::ErrorDescriptor& description)
{
    error = description.code();
    errorString = description.text();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Result), (ubjson)(xml)(json)(csv_record), _Fields)

JsonResult::JsonResult(const Result& result):
    Result(result)
{
}

void JsonResult::writeError(QByteArray* outBody, Error error, const QString& errorMessage)
{
    JsonResult jsonRestResult;
    jsonRestResult.setError(error, errorMessage);
    *outBody = QJson::serialized(jsonRestResult);
}

UbjsonResult::UbjsonResult(const Result& result):
    Result(result)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((JsonResult)(UbjsonResult), (ubjson)(json), _Fields)

} // namespace nx::network::rest

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::network::rest::Result, Error,
    (nx::network::rest::Result::NoError, "OK")
    (nx::network::rest::Result::MissingParameter, "Required parameter is missing")
    (nx::network::rest::Result::InvalidParameter, "Invalid parameter value")
    (nx::network::rest::Result::CantProcessRequest, "Internal server error")
    (nx::network::rest::Result::Forbidden, "Access denied")
)

// Dummy methods to make the Fusion macro compile for json and ubjson at once.

static void serialize(const QJsonValue& /*value*/, QnUbjsonWriter<QByteArray>* /*stream*/)
{
    NX_ASSERT(false, "We should not serialize QJsonValue to UBJson.");
}

static bool deserialize(QnUbjsonReader<QByteArray>* /*stream*/, QJsonValue* /*target*/)
{
    NX_ASSERT(false, "We should not serialize QJsonValue to UBJson.");
    return true;
}
