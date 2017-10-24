#pragma once

#include "rest/server/json_rest_handler.h"

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
    nx::vms::cloud_integration::CloudManagerGroup* m_cloudManagerGroup;

    bool authorize(
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult* result,
        nx_http::StatusCode::Value* const authorizationStatusCode);
    bool validateInputData(
        QnCommonModule* commonModule,
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool checkInternetConnection(QnCommonModule* commonModule, QnJsonRestResult* result);

    bool saveCloudData(
        QnCommonModule* commonModule,
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool saveCloudCredentials(
        QnCommonModule* commonModule,
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool insertCloudOwner(
        QnCommonModule* commonModule,
        const CloudCredentialsData& data,
        QnJsonRestResult* result);

    bool fetchNecessaryDataFromCloud(
        QnCommonModule* commonModule,
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool saveLocalSystemIdToCloud(
        QnCommonModule* commonModule,
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool initializeCloudRelatedManagers(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);

    bool rollback(QnCommonModule* commonModule);
};
