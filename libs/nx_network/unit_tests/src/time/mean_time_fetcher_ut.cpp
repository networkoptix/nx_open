#include "basic_time_protocol_client_tests.h"

#include <nx/network/time/mean_time_fetcher.h>
#include <nx/network/time/time_protocol_client.h>

namespace nx {
namespace network {
namespace test {

class MeanTimeFetcher:
    public network::MeanTimeFetcher
{
    using base_type = network::MeanTimeFetcher;

public:
    MeanTimeFetcher(const SocketAddress& serverEndpoint)
    {
        for (int i = 0; i < 3; ++i)
            addTimeFetcher(std::make_unique<TimeProtocolClient>(serverEndpoint));
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    MeanTimeFetcher,
    BasicTimeProtocolClient,
    MeanTimeFetcher);

} // namespace test
} // namespace network
} // namespace nx
