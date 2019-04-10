#pragma once

#include <nx/network/rest/handler.h>

namespace nx::vms::server::rest {

class ServerTimeHandler: public nx::network::rest::Handler
{
public:
    ServerTimeHandler(const QString &getTimeHandlerPath);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

private:
    QString m_getTimeHandlerPath;
};

} // namespace nx::vms::server::rest
