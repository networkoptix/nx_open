#pragma once

#include <QString>

#include <core/resource/resource_fwd.h>

QString extractHost(const QString &url);
QString getFullResourceName(const QnResourcePtr& resource, bool showIp);
inline QString getShortResourceName(const QnResourcePtr& resource) { return getFullResourceName(resource, false); }

/**
 * @brief Calculate common name for the target devices list.
 * @details Following rules are applied:
 * * If all devices are cameras - "%n Cameras";
 * * If all devices are IO Modules - "%n IO Modules";
 * * Supported single items - "Camera" or "IO Module";
 * * For mixed or empty list "Devices" is used;
 * @param capitalize Should the word begin from the capital letter, default value is true.
 */
QString getDevicesName(const QnVirtualCameraResourceList &devices = QnVirtualCameraResourceList(), bool capitalize = true);

inline QString getDevicesNameUpper(const QnVirtualCameraResourceList &devices = QnVirtualCameraResourceList()) {
    return getDevicesName(devices, true);
}

inline QString getDevicesNameLower(const QnVirtualCameraResourceList &devices = QnVirtualCameraResourceList()) {
    return getDevicesName(devices, false);
}

QString selectDevice();
