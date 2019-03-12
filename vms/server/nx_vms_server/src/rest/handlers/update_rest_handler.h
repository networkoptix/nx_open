#pragma once

#include "rest/server/json_rest_handler.h"
#include <nx/vms/api/data/software_version.h>

class QnServerUpdateTool;

class QnUpdateRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnUpdateRestHandler(
        QnServerUpdateTool* updateTool, 
        const nx::vms::api::SoftwareVersion& engineVersion);

    virtual int executeGet(
        const QString &path, 
        const QnRequestParams &params, 
        QnJsonRestResult &result, 
        const QnRestConnectionProcessor *processor) override;

    virtual int executePost(
        const QString &path, 
        const QnRequestParams &params, 
        const QByteArray &body, 
        QnJsonRestResult &result, 
        const QnRestConnectionProcessor*processor) override;

private:
    QnServerUpdateTool* m_updateTool = nullptr;
    nx::vms::api::SoftwareVersion m_engineVersion;

    int handlePartialUpdate(
        const QString &updateId, 
        const QByteArray &data, 
        qint64 offset, 
        QnJsonRestResult &result);
};
