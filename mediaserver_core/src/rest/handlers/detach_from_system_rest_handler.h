#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>

struct PasswordData;
namespace nx { namespace vms { namespace cloud_integration { class CloudConnectionManager; } } }
namespace ec2 {
    class AbstractTransactionMessageBus;
}

class QnDetachFromSystemRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnDetachFromSystemRestHandler(
        nx::vms::cloud_integration::CloudConnectionManager* const cloudConnectionManager,
        ec2::AbstractTransactionMessageBus* messageBus);

    virtual int executeGet(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path, const QnRequestParams& params, const QByteArray& body,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;

private:
    int execute(
        PasswordData passwordData,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult& result);
private:
    nx::vms::cloud_integration::CloudConnectionManager* const m_cloudConnectionManager;
    ec2::AbstractTransactionMessageBus* m_messageBus;
};
