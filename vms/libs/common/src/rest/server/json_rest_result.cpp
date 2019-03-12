#include "json_rest_result.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    QnRestResult, Error,
    (QnRestResult::NoError, "OK")
    (QnRestResult::MissingParameter, "Required parameter is missing")
    (QnRestResult::InvalidParameter, "Invalid parameter value")
    (QnRestResult::CantProcessRequest, "Internal server error")
    (QnRestResult::Forbidden, "Access denied")
)

QN_FUSION_DEFINE_FUNCTIONS(QnRestResult::Error, (numeric))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnRestResult, (ubjson)(json)(xml)(csv_record), QnRestResult_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_REST_RESULT_TYPES, (ubjson)(json), _Fields)

QnRestResult::ErrorDescriptor::ErrorDescriptor(
    QnRestResult::Error errorCode,
    QString argument)
    :
    m_errorCode(errorCode)
{
    m_arguments.push_back(std::move(argument));
}

QnRestResult::ErrorDescriptor::ErrorDescriptor(
    Error errorCode,
    QStringList arguments)
    :
    m_errorCode(errorCode),
    m_arguments(std::move(arguments))
{
}

QnRestResult::Error QnRestResult::ErrorDescriptor::errorCode() const
{
    return m_errorCode;
}

QString QnRestResult::ErrorDescriptor::text() const
{
    switch (m_errorCode)
    {
        case NoError:
            return "OK";
        case MissingParameter:
            return lm("Missing required parameter '%1'").arg(m_arguments.value(0));
        case InvalidParameter:
            return lm("Invalid parameter '%1' value: '%2'").args(
                m_arguments.value(0), m_arguments.value(1));
        case CantProcessRequest:
            return lm("Failed to process request '%1'").arg(m_arguments.value(0));
        case Forbidden:
            return "Forbidden";
        case BadRequest:
            return "Bad request";
        case InternalServerError:
            return "Internal server error";
        default:
            return lm("Unknown error code '%1'. Arguments: %2")
                .args(static_cast<int>(m_errorCode), m_arguments.join(", "));
    }
}

QnRestResult::QnRestResult():
    error(NoError)
{
}

void QnRestResult::setError(Error errorValue, const QString& errorStringValue)
{
    error = errorValue;
    errorString = errorStringValue;
}

void QnRestResult::setError(const QnRestResult::ErrorDescriptor& errorDescriptor)
{
    error = errorDescriptor.errorCode();
    errorString = errorDescriptor.text();
}

QnJsonRestResult::QnJsonRestResult(): QnRestResult()
{
}

QnJsonRestResult::QnJsonRestResult(const QnRestResult& base): QnRestResult(base)
{
}

QnUbjsonRestResult::QnUbjsonRestResult(): QnRestResult()
{
}

QnUbjsonRestResult::QnUbjsonRestResult(const QnRestResult& base): QnRestResult(base)
{
}

// Dummy methods to make the Fusion macro compile for json and ubjson at once.

void serialize(const QJsonValue& /*value*/, QnUbjsonWriter<QByteArray>* /*stream*/)
{
    NX_ASSERT(false, "We should not serialize QJsonValue to UBJson.");
}

bool deserialize(QnUbjsonReader<QByteArray>* /*stream*/, QJsonValue* /*target*/)
{
    NX_ASSERT(false, "We should not serialize QJsonValue to UBJson.");
    return true;
}
