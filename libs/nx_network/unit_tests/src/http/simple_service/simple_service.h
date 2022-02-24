// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/service.h>

#include "view.h"

namespace nx::network::http::server::test {

class View;

class SimpleService: public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    SimpleService(int argc, char** argv);

    std::vector<network::SocketAddress> httpEndpoints() const;
    void setSuccessfullAttemptNum(unsigned num) { m_view->setSuccessfullAttemptNum(num); };
    void resetAttemptsNum() { m_view->resetAttemptsNum(); };
    unsigned getCurAttempNum() { return m_view->getCurAttempNum(); }

protected:
    virtual std::unique_ptr<nx::utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const nx::utils::AbstractServiceSettings& settings) override;

private:
    View* m_view = nullptr;
};

} // namespace nx::network::http::server::test
