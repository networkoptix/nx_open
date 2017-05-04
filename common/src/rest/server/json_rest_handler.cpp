#include "json_rest_handler.h"
#include <utils/common/util.h>

namespace {
    const static QLatin1String kExtraFormatting("extraFormatting");
} // namespace

QnJsonRestHandler::QnJsonRestHandler():
    m_contentType("application/json")
{
}

QnJsonRestHandler::~QnJsonRestHandler()
{
}

int QnJsonRestHandler::executeGet(
    const QString& path, const QnRequestParamList& params, QByteArray& result,
    QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    QnJsonRestResult jsonResult;
    const auto paramsMap = processParams(params);
    int returnCode = executeGet(path, paramsMap, jsonResult, owner);

    result = QJson::serialized(jsonResult);
    if (paramsMap.contains(kExtraFormatting))
        result  = formatJSonString(result);

    contentType = m_contentType;

    return returnCode;
}

int QnJsonRestHandler::executePost(
    const QString& path, const QnRequestParamList& params, const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnJsonRestResult jsonResult;
    const auto paramsMap = processParams(params);
    int returnCode = executePost(path, paramsMap, body, jsonResult, owner);

    result = QJson::serialized(jsonResult);
    if (paramsMap.contains(kExtraFormatting))
        result  = formatJSonString(result);
    contentType = m_contentType;

    return returnCode;
}

int QnJsonRestHandler::executePost(
    const QString& path, const QnRequestParams& params, const QByteArray& /*body*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, owner);
}

int QnJsonRestHandler::executeGet(
    const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    return executePost(path, params, QByteArray(), result, owner);
}

QnRequestParams QnJsonRestHandler::processParams(const QnRequestParamList& params) const
{
    QnRequestParams result;
    for (const QnRequestParam& param: params)
        result.insertMulti(param.first, param.second);
    return result;
}
