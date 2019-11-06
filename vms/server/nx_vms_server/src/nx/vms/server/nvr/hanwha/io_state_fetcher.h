#pragma once

#include <set>
#include <functional>

#include <nx/utils/thread/long_runnable.h>
#include <api/model/api_ioport_data.h>

namespace nx::vms::server::nvr::hanwha {

class IoStateFetcher: public QnLongRunnable
{
public:
    IoStateFetcher(std::function<void(const std::set<QnIOStateData>&)> stateHandler);
    virtual ~IoStateFetcher();

protected:
    virtual void run() override;

private:
    std::function<void(const std::set<QnIOStateData>&)> m_stateHandler;
};

} // namespace nx::vms::server::nvr::hanwha
