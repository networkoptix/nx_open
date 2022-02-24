// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "simple_service.h"

#include "view.h"
#include <nx/branding.h>
#include <nx/network/http/server/settings.h>

namespace {

static const QString kApplicationDisplayName = nx::branding::company() + "Simple Service";

} // namespace

namespace nx::network::http::server::test {

std::unique_ptr<nx::utils::AbstractServiceSettings> SimpleService::createSettings()
{
    return std::make_unique<Settings>();
}

std::vector<network::SocketAddress> SimpleService::httpEndpoints() const
{
    return m_view->httpEndpoints();
}

SimpleService::SimpleService(int argc, char** argv): base_type(argc, argv, kApplicationDisplayName)
{
}

int SimpleService::serviceMain(const nx::utils::AbstractServiceSettings& abstractSettings)
{
    const Settings& settings = static_cast<const Settings&>(abstractSettings);
    View view(settings);
    m_view = &view;
    view.start();
    int result = runMainLoop();
    return result;
}

} // namespace  nx::network::http::server::test
