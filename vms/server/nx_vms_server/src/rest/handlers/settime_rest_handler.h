#pragma once

#include <nx/network/rest/handler.h>

namespace nx::vms::server {

struct SetTimeData;

class SetTimeRestHandler: public nx::network::rest::Handler
{
protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::rest::Response execute(
        const SetTimeData& data,
        const QnRestConnectionProcessor* owner);
};

} // namespace nx::vms::server
