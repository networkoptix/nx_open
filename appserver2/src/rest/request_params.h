#pragma once

#include <utils/common/id.h>
#include <utils/common/request_param.h>
#include <nx_ec/data/api_fwd.h>

#include "request_type_wrappers.h"

class QUrlQuery;
class QnCommonModule;

namespace ec2 {

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, QString* value);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, ApiStoredFilePath* value);
void toUrlParams(const ApiStoredFilePath& id, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, QnUuid* id);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, QnCameraUuid* id);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, QnLayoutUuid* id);

void toUrlParams(const QnUuid& id, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, QnStorageParentId* id);
void toUrlParams(const QnStorageParentId& id, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, Qn::SerializationFormat* format);
void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, ApiLoginData* loginInfo);
void toUrlParams(const ApiLoginData&, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, ApiTranLogFilter* tranLogFilter);
void toUrlParams(const ApiTranLogFilter&, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, nullptr_t*);
void toUrlParams(const nullptr_t&, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList &params, QByteArray *value);
void toUrlParams(
    const QByteArray &filter, QUrlQuery *query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command,
    const QnRequestParamList& params,
    QnCameraDataExQuery* query);
void toUrlParams(
    const QnCameraDataExQuery& filter, QUrlQuery *query);


} // namespace ec2
