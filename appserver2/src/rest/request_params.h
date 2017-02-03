#pragma once

#include <utils/common/id.h>
#include <utils/common/request_param.h>
#include <nx_ec/data/api_fwd.h>

class QUrlQuery;

namespace ec2 {

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, QString* value);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, ApiStoredFilePath* value);
void toUrlParams(const ApiStoredFilePath& id, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, QnUuid* id);
void toUrlParams(const QnUuid& id, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, ParentId* id);
void toUrlParams(const ParentId& id, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, Qn::SerializationFormat* format);
void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, ApiLoginData* loginInfo);
void toUrlParams(const ApiLoginData&, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, ApiTranLogFilter* tranLogFilter);
void toUrlParams(const ApiTranLogFilter&, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList& params, nullptr_t*);
void toUrlParams(const nullptr_t&, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command, const QnRequestParamList &params, QByteArray *value);
void toUrlParams(
    const QByteArray &filter, QUrlQuery *query);
} // namespace ec2
