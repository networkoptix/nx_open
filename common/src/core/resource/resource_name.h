#pragma once

#include <QString>

#include <core/resource/resource_fwd.h>

QString extractHost(const QString &url);
QString getFullResourceName(const QnResourcePtr& resource, bool showIp);
inline QString getShortResourceName(const QnResourcePtr& resource) { return getFullResourceName(resource, false); }

/**
 * @brief Calculate default devices name.
 * @details Following rules are applied:
 * * If all devices in the system are cameras - "Cameras";
 * * Otherwise "Devices" is used;
 * @param plural        Should the word be in the plural form, default value is true.
 * @param capitalize    Should the word begin from the capital letter, default value is true.
 */
QString getDefaultDevicesName(bool plural = true, bool capitalize = true);

/**
 * @brief Calculate default devices name for the given devices list.
 * @details Following rules are applied:
 * * If all devices in the list are cameras - "Cameras";
 * * If all devices in the list are IO modules - "IO Modules";
 * * Otherwise "Devices" is used;
 * @param plural        Should the word be in the plural form, default value is true.
 * @param capitalize    Should the word begin from the capital letter, default value is true.
 */
QString getDefaultDevicesName(const QnVirtualCameraResourceList &devices, bool capitalize = true);

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
