#pragma once

#include <nx/utils/thread/stoppable.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cloud {
namespace gateway {

class VmsGatewayProcess;

class NX_VMS_GATEWAY_API VmsGatewayProcessPublic:
    public QnStoppable
{
public:
    VmsGatewayProcessPublic(int argc, char **argv);
    virtual ~VmsGatewayProcessPublic();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    VmsGatewayProcess* impl() const;

private:
    VmsGatewayProcess* m_impl;
};

}   //namespace gateway
}   //namespace cloud
}   //namespace nx
