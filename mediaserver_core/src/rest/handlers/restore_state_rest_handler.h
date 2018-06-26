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
    // virtual int executeGet for RestoreState request must not be overriden.
    // Parent's method returns badRequest for this request.

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

    bool verifyCurrentPassword(
        const QnRequestParams& params,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult* result = nullptr);
};
