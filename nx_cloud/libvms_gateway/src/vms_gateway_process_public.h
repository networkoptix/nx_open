#pragma once

#include <utils/common/stoppable.h>

namespace nx {
namespace cloud {
namespace gateway {

class VmsGatewayProcess;

class VmsGatewayProcessPublic
:
    public QnStoppable
{
public:
    static constexpr size_t kMaxStartRetryCount = 1;

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
