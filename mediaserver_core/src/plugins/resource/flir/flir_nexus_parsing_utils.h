#pragma once

#include <boost/optional.hpp>

#include "flir_nexus_common.h"

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

boost::optional<Notification> parseNotification(const QString& notificationString);

boost::optional<quint64> parseNotificationDateTime(const QString& dateTimeString);

boost::optional<Subscription> parseSubscription(const QString& subscriptionString);

boost::optional<ServerStatus> parseNexusServerStatusResponse(const QString& response);

boost::optional<PrivateDeviceInfo> parsePrivateDeviceInfo(const QString& deviceInfoString);

boost::optional<DeviceDiscoveryInfo> parseDeviceDiscoveryInfo(const QString& deviceDiscoveryInfoString);

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx