#pragma once

#include <core/resource_access/user_access_data.h>
#include <rest/server/json_rest_handler.h>

struct DetachFromCloudData;
namespace nx { namespace vms { namespace cloud_integration { struct CloudManagerGroup; } } }

class QnDetachFromCloudRestHandler:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnDetachFromCloudRestHandler(
        nx::vms::cloud_integration::CloudManagerGroup* const cloudManagerGroup);

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    int execute(
        DetachFromCloudData passwordData,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult& result);

private:
    nx::vms::cloud_integration::CloudManagerGroup* m_cloudManagerGroup;
};
