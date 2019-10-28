#include "live_stream_provider_mock.h"

namespace nx {
namespace vms::server {
namespace resource {
namespace test {


void LiveStreamProviderMock::stop()
{
    exit(); //< Leave QT event loop (it is QThread default behaviour is there is no custom run()).
    QnLiveStreamProvider::stop();
}

} // namespace test
} // namespace resource
} // namespace vms::server
} // namespace nx

