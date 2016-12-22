#pragma once

#include <utils/common/stoppable.h>

namespace nx {
namespace cdb {

class CloudDBProcess;

class CloudDBProcessPublic
:
    public QnStoppable
{
public:
    static constexpr size_t kMaxStartRetryCount = 1;

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
