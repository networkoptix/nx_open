#pragma once

#include <nx/utils/move_only_func.h>
#include <utils/common/stoppable.h>

namespace nx {
namespace hpm {

class MediatorProcess;

class MediatorProcessPublic
:
    public QnStoppable
{
public:
    //< As STUN have to take the same TCP and UDP port.
    static constexpr size_t kMaxStartRetryCount = 3;

    MediatorProcessPublic(int argc, char **argv);
    virtual ~MediatorProcessPublic();

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    virtual void pleaseStop() override;

    int exec();

    const MediatorProcess* impl() const;

private:
    MediatorProcess* m_impl;
};

} // namespace hpm
} // namespace nx
