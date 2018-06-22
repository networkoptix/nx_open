#pragma once

#include "id_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DiscoverPeerData: Data
{
    QString url;
    QnUuid id;
};
#define DiscoverPeerData_Fields \
    (url)(id)

struct NX_VMS_API DiscoveryData: IdData
{
    QString url;
    bool ignore = false;
};
#define DiscoveryData_Fields \
    IdData_Fields (url)(ignore)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::DiscoverPeerData)
Q_DECLARE_METATYPE(nx::vms::api::DiscoveryData)
