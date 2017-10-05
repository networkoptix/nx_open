#pragma once

#include <rest/server/json_rest_handler.h>
#include <network/module_information.h>

class QnPingSystemRestHandler : public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;

private:
    QnModuleInformation remoteModuleInformation(const nx::utils::Url &url, const QString&  getKey, int &status);
};
