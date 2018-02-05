#include "p2p_sync_settings.h"

#include <nx/utils/settings.h>

namespace nx {
namespace cdb {
namespace ec2 {

namespace {

static const QLatin1String kMaxConcurrentConnectionsFromSystem(
    "p2pDb/maxConcurrentConnectionsFromSystem");
constexpr const unsigned int kMaxConcurrentConnectionsFromSystemDefault = 2;

} // namespace

Settings::Settings():
    maxConcurrentConnectionsFromSystem(
        kMaxConcurrentConnectionsFromSystemDefault)
{
}

void Settings::load(const QnSettings& settings)
{
    maxConcurrentConnectionsFromSystem = settings.value(
        kMaxConcurrentConnectionsFromSystem,
        kMaxConcurrentConnectionsFromSystemDefault).toInt();
}

} // namespace ec2
} // namespace cdb
} // namespace nx
