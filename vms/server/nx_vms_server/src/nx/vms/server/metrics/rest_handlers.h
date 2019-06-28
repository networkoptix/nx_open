#pragma once

#include <core/resource_management/resource_pool.h>
#include <nx/vms/utils/metrics/controller.h>
#include <nx/network/rest/handler.h>

namespace nx::vms::server::metrics {

class LocalRestHandler: public nx::network::rest::Handler
{
public:
    LocalRestHandler(utils::metrics::Controller* controller);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

protected:
    utils::metrics::Controller* const m_controller;
};

class SystemRestHandler: public LocalRestHandler
{
public:
    SystemRestHandler(utils::metrics::Controller* controller, QnResourcePool* resourcePool);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

protected:
    QnResourcePool* const m_resourcePool;
};

} // namespace nx::vms::server
