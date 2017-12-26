#pragma once

#include <rest/server/json_rest_handler.h>

namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {

static const QString kUpdates2ActionPath = "/api/updates2";


class Updates2ActionRestHandler: public QnJsonRestHandler
{
public:
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
    virtual JsonRestResponse executeDelete(const JsonRestRequest& request) override;
    virtual JsonRestResponse executePost(
        const JsonRestRequest& request,
        const QByteArray& body) override;
    virtual JsonRestResponse executePut(
        const JsonRestRequest& request,
        const QByteArray& body) override;

private:
};

} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx
