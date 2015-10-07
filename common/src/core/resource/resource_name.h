#pragma once

#include <QString>

#include <core/resource/resource_fwd.h>

QString extractHost(const QString &url);
QString getFullResourceName(const QnResourcePtr& resource, bool showIp);
inline QString getShortResourceName(const QnResourcePtr& resource) { return getFullResourceName(resource, false); }

/** Shortcut for single device or singular form. */
QString getDefaultDeviceNameLower(const QnVirtualCameraResourcePtr &device = QnVirtualCameraResourcePtr());

/** Shortcut for single device or singular form. */
QString getDefaultDeviceNameUpper(const QnVirtualCameraResourcePtr &device = QnVirtualCameraResourcePtr());

/**
 * @brief Calculate common name for the target devices list.
 * @details Following rules are applied:
 * * If all devices are cameras - "%n Cameras";
 * * If all devices are IO Modules - "%n IO Modules";
 * * For mixed list "%n Devices" is used;
 * @param capitalize Should the word begin from the capital letter, default value is true.
 */
QString getNumericDevicesName(const QnVirtualCameraResourceList &devices, bool capitalize = true);
