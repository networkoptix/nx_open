#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource/resource_fwd.h>

class QnMergeSystemsRestHandler : public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
            const QString &path,
            const QnRequestParams &params,
            QnJsonRestResult &result,
            const QnRestConnectionProcessor *owner) override;

private:
    bool applyCurrentSettings(
            const QUrl &remoteUrl,
            const QString& remoteAuthKey,
            bool oneServer,
            const QnRestConnectionProcessor *owner);

    bool applyRemoteSettings(
            const QUrl &remoteUrl,
            const QString &systemName,
            const QString& remoteAuthKey,
            const QnRestConnectionProcessor *owner);
};
