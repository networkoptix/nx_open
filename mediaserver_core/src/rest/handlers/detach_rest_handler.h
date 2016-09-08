#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_management/user_access_data.h>

struct DetachFromCloudData;
class CloudConnectionManager;

class QnDetachFromCloudRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnDetachFromCloudRestHandler(CloudConnectionManager* const cloudConnectionManager);

    virtual int executeGet(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path, const QnRequestParams& params, const QByteArray& body,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;

private:
    int execute(
        DetachFromCloudData passwordData,
        const Qn::UserAccessData& accessRights,
        QnJsonRestResult& result);

private:
    CloudConnectionManager* const m_cloudConnectionManager;
};
