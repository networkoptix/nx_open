#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource/resource_fwd.h>
#include <utils/merge_systems_common.h>
#include <media_server/serverutil.h>

struct MergeSystemData;

namespace ec2 {
    class QnTransactionMessageBusBase;
}

class QnMergeSystemsRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnMergeSystemsRestHandler(ec2::QnTransactionMessageBusBase* messageBus);

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

    void setMergeError(
        QnJsonRestResult& result,
        utils::MergeSystemsStatus::Value mergeStatus);

    bool executeRemoteConfigure(
        const ConfigureSystemData& data,
        const QUrl &remoteUrl,
        const QString& postKey,
        const QnRestConnectionProcessor* owner);
private:
    ec2::QnTransactionMessageBusBase* m_messageBus;
};
