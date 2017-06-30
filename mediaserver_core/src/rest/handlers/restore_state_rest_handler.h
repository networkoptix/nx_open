#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>

struct PasswordData;

/**
 * Restore media server state to default:
 * 1. Reset system name
 * 2. Restore database state
 */

class QnRestoreStateRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;
    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;
private:
    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner) override;
    int execute(
        PasswordData passwordData,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult& result);
};
