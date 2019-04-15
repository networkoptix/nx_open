#pragma once

#include <nx/network/rest/handler.h>

namespace nx::vms::server {

class ExecScript: public nx::network::rest::Handler
{
public:
    ExecScript(const QString& dataDirectory);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

    virtual void afterExecute(
        const nx::network::rest::Request& request,
        const nx::network::rest::Response& response) override;

private:
    const QString m_dataDirectory;
};

} // namespace nx::vms::server
