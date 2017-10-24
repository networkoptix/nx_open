#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>

struct DetachFromCloudData;
namespace nx { namespace vms { namespace cloud_integration { class CloudConnectionManager; } } }

class QnDetachFromCloudRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnDetachFromCloudRestHandler(
        nx::vms::cloud_integration::CloudConnectionManager* const cloudConnectionManager);

    virtual int executeGet(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path, const QnRequestParams& params, const QByteArray& body,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;

private:
    int execute(
        DetachFromCloudData passwordData,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult& result);

private:
    nx::vms::cloud_integration::CloudConnectionManager* const m_cloudConnectionManager;
};
