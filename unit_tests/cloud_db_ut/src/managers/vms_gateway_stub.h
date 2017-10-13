#pragma once

#include <queue>
#include <set>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/managers/vms_gateway.h>

namespace nx {
namespace cdb {
namespace test {

struct MergeRequestParameters
{
    std::string username;
    std::string targetSystemId;
};

class VmsGatewayStub:
    public AbstractVmsGateway
{
public:
    virtual ~VmsGatewayStub() override;

    virtual void merge(
        const std::string& username,
        const std::string& targetSystemId,
        VmsRequestCompletionHandler completionHandler) override;

    void pause();
    void resume();

    MergeRequestParameters popRequest();
    
    void failEveryRequestToSystem(const std::string& id);

private:
    struct RequestContext
    {
        MergeRequestParameters params;
        VmsRequestCompletionHandler completionHandler;
    };

    nx::network::aio::BasicPollable m_pollable;
    bool m_paused = false;
    std::queue<RequestContext> m_queuedRequests;
    std::set<std::string> m_systemsRequested;
    std::set<std::string> m_malfunctioningSystems;
    nx::utils::SyncQueue<MergeRequestParameters> m_performedRequestsQueue;

    void reportRequestResult(RequestContext requestContext);
};

} // namespace test
} // namespace cdb
} // namespace nx
