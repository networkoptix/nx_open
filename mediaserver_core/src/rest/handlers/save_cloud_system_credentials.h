#pragma once

#include "rest/server/json_rest_handler.h"

struct CloudCredentialsData;
struct CloudManagerGroup;

class QnSaveCloudSystemCredentialsHandler:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnSaveCloudSystemCredentialsHandler(CloudManagerGroup* cloudManagerGroup);

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
    CloudManagerGroup* m_cloudManagerGroup;

    bool authorize(
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult* result,
        nx_http::StatusCode::Value* const authorizationStatusCode);
    bool validateInputData(const CloudCredentialsData& data, QnJsonRestResult* result);
    bool checkInternetConnection(QnJsonRestResult* result);

    bool saveCloudData(const CloudCredentialsData& data, QnJsonRestResult* result);
    bool saveCloudCredentials(const CloudCredentialsData& data, QnJsonRestResult* result);
    bool insertCloudOwner(const CloudCredentialsData& data, QnJsonRestResult* result);

    bool fetchNecessaryDataFromCloud(const CloudCredentialsData& data, QnJsonRestResult* result);
    bool saveLocalSystemIdToCloud(const CloudCredentialsData& data, QnJsonRestResult* result);
    bool initializeCloudRelatedManagers(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);

    bool rollback();
};
