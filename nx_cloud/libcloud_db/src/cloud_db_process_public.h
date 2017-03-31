#pragma once

#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/barrier_handler.h>

namespace nx {
namespace cdb {

class CloudDBProcess;

class CloudDBProcessPublic:
    public QnStoppable
{
public:
    CloudDBProcessPublic(int argc, char **argv);
    virtual ~CloudDBProcessPublic();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    const CloudDBProcess* impl() const;

private:
    CloudDBProcess* m_impl;
};

} // namespace cdb
} // namespace nx
