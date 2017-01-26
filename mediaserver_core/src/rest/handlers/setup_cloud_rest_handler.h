#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>

struct CloudManagerGroup;
struct SetupRemoveSystemData;

class QnSetupCloudSystemRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnSetupCloudSystemRestHandler(CloudManagerGroup* cloudManagerGroup);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
private:
    CloudManagerGroup* m_cloudManagerGroup;

    int execute(
        SetupRemoveSystemData data,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult &result);
};
