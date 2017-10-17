#pragma once

#include <api/model/configure_system_data.h>
#include <rest/server/json_rest_handler.h>
#include <core/resource/resource_fwd.h>
#include <utils/merge_systems_common.h>

struct MergeSystemData;

namespace ec2 {
    class AbstractTransactionMessageBus;
}

class QnMergeSystemsRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnMergeSystemsRestHandler(ec2::AbstractTransactionMessageBus* messageBus);

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
        const nx::utils::Url &remoteUrl,
        const QString& getKey,
        const QString& postKey,
        bool oneServer,
        const QnRestConnectionProcessor* owner);

    bool applyRemoteSettings(
        const nx::utils::Url &remoteUrl,
        const QnUuid& systemId,
        const QString& systemName,
        const QString& getKey,
        const QString& postKey,
        const QnRestConnectionProcessor* owner);

    void setMergeError(
        QnJsonRestResult& result,
        utils::MergeSystemsStatus::Value mergeStatus);

    bool executeRemoteConfigure(
        const ConfigureSystemData& data,
        const nx::utils::Url &remoteUrl,
        const QString& postKey,
        const QnRestConnectionProcessor* owner);
private:
    ec2::AbstractTransactionMessageBus* m_messageBus;
};
