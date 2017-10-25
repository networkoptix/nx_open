#pragma once

#include <core/resource_access/user_access_data.h>
#include <rest/server/json_rest_handler.h>

struct CloudCredentialsData;
namespace nx { namespace vms { namespace cloud_integration { struct CloudManagerGroup; } } }
class QnCommonModule;

class QnSaveCloudSystemCredentialsHandler:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnSaveCloudSystemCredentialsHandler(
        nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup);

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*);

    int execute(
        const CloudCredentialsData& data,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

private:
    QnCommonModule* m_commonModule = nullptr;
    nx::vms::cloud_integration::CloudManagerGroup* m_cloudManagerGroup = nullptr;

    bool authorize(
        const Qn::UserAccessData& accessRights,
        QnJsonRestResult* result,
        nx_http::StatusCode::Value* const authorizationStatusCode);
};
