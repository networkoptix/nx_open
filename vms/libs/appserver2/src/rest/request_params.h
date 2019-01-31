#pragma once

#include <utils/common/id.h>
#include <utils/common/request_param.h>
#include <nx_ec/data/api_fwd.h>

#include "request_type_wrappers.h"
#include <nx/vms/api/data/media_server_data.h>
#include <nx_ec/data/api_statistics.h>

class QUrlQuery;
class QnCommonModule;

namespace ec2 {

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, QString* value);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, nx::vms::api::StoredFilePath* value);
void toUrlParams(const nx::vms::api::StoredFilePath& id, QUrlQuery* query);

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
    const QString& command, const QnRequestParamList& params, nx::vms::api::StorageParentId* id);
void toUrlParams(const nx::vms::api::StorageParentId& id, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, Qn::SerializationFormat* format);
void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command,
    const QnRequestParamList& params,
    nx::vms::api::ConnectionData* loginInfo);

void toUrlParams(const nx::vms::api::ConnectionData&, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, ApiTranLogFilter* tranLogFilter);
void toUrlParams(const ApiTranLogFilter&, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params, std::nullptr_t*);
void toUrlParams(const std::nullptr_t&, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList &params, QByteArray *value);
void toUrlParams(
    const QByteArray &filter, QUrlQuery *query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command, const QnRequestParamList& params,
    ApiStatisticsServerArguments* arguments);
void toUrlParams(
    const ApiStatisticsServerArguments& arguments, QUrlQuery* query);

bool parseHttpRequestParams(
    QnCommonModule* commonModule,
    const QString& command,
    const QnRequestParamList& params,
    QnCameraDataExQuery* query);
void toUrlParams(
    const QnCameraDataExQuery& filter, QUrlQuery *query);

} // namespace ec2
