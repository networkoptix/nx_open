#pragma once

#include <boost/optional.hpp>

#include "flir_nexus_common.h"
#include "flir_fc_private.h"

namespace nx {
namespace plugins {
namespace flir {

boost::optional<nexus::Notification> parseNotification(const QString& notificationString);

boost::optional<quint64> parseNotificationDateTime(const QString& dateTimeString);

boost::optional<nexus::Subscription> parseSubscription(const QString& subscriptionString);

boost::optional<fc_private::ServerStatus> parseNexusServerStatusResponse(const QString& response);

boost::optional<fc_private::DeviceInfo> parsePrivateDeviceInfo(const QString& deviceInfoString);

boost::optional<nexus::DeviceDiscoveryInfo> parseDeviceDiscoveryInfo(const QString& deviceDiscoveryInfoString);

} // namespace flir
} // namespace plugins
} // namespace nx