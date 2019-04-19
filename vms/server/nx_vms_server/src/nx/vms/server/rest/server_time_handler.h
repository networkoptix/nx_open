#pragma once

#include <rest/server/json_rest_handler.h>

namespace nx::vms::server::rest {

class ServerTimeHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    ServerTimeHandler(const QString &getTimeHandlerPath);

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    QString m_getTimeHandlerPath;
};

} // namespace nx::vms::server::rest