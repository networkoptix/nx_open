#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource/resource_fwd.h>

struct MergeSystemData;

class QnMergeSystemsRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor *owner) override;

    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*owner) override;
private:
    int execute(
        MergeSystemData data,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult &result);

    bool applyCurrentSettings(
        const QUrl &remoteUrl,
        const QString& getKey,
        const QString& postKey,
        bool oneServer,
        const QnRestConnectionProcessor* owner);

    bool applyRemoteSettings(
        const QUrl &remoteUrl,
        const QnUuid& systemId,
        const QString& getKey,
        const QString& postKey,
        const QnRestConnectionProcessor* owner);
};
