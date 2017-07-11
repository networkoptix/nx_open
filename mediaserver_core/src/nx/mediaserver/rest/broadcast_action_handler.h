#pragma once

#include <rest/server/fusion_rest_handler.h>

namespace nx {
namespace mediaserver {
namespace rest {

class BroadcastActionHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* processor) override;

};

} // namespace nx
} // namespace mediaserver
} // namespace rest
