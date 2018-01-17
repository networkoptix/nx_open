#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/stoppable.h>

namespace nx {
namespace hpm {

class MediatorProcess;

class MediatorProcessPublic:
    public QnStoppable
{
public:
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
