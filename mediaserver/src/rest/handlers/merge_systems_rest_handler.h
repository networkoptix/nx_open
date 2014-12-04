#ifndef MERGE_SYSTEMS_REST_HANDLER_H
#define MERGE_SYSTEMS_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>
#include <core/resource/resource_fwd.h>

class QnMergeSystemsRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) override;
private:
    bool applyCurrentSettings(const QUrl &remoteUrl, const QString &user, const QString &password, const QString &currentPassword, const QnUserResourcePtr &admin);
    bool applyRemoteSettings(const QUrl &remoteUrl, const QString &systemName, const QString &user, const QString &password, QnUserResourcePtr &admin);
};

#endif // MERGE_SYSTEMS_REST_HANDLER_H
