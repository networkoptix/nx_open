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

private:
    ec2::AbstractTransactionMessageBus* m_messageBus;
};
