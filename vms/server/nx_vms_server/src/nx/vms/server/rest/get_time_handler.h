#pragma once

#include <rest/server/json_rest_handler.h>

namespace nx::vms::server::rest {

class GetTimeHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;
};

} // namespace nx::vms::server::rest