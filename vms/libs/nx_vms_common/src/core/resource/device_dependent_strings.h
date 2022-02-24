// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <core/resource/resource_fwd.h>

enum QnCameraDeviceType
{
    Mixed,
    Camera,
    IOModule,

    Count
};

class NX_VMS_COMMON_API QnCameraDeviceStringSet
{
public:
    QnCameraDeviceStringSet();

    QnCameraDeviceStringSet(
        const QString& mixedString,
        const QString& cameraString,
        const QString& ioModuleString = QString());

    QnCameraDeviceStringSet(
        const QString& mixedSigularString,
        const QString& mixedPluralString,
        const QString& cameraSigularString,
        const QString& cameraPluralString,
        const QString& ioModuleSigularString,
        const QString& ioModulePluralString);

    QString getString(QnCameraDeviceType deviceType, bool plural = false) const;
    void setString(QnCameraDeviceType deviceType, bool plural, const QString& value);

    bool isValid() const;

private:
    std::array<QString, QnCameraDeviceType::Count> m_singularStrings;
    std::array<QString, QnCameraDeviceType::Count> m_pluralStrings;
};

class NX_VMS_COMMON_API QnDeviceDependentStrings
{
    QnDeviceDependentStrings() = delete;

public:
    /**
     * @brief Calculate common name for the target devices list.
     * @details Following rules are applied:
     * * If all devices are cameras - "%n Cameras";
     * * If all devices are I/O Modules - "%n I/O Modules";
     * * For mixed list "%n Devices" is used;
     * @param capitalize Should the word begin from the capital letter, default value is true.
     */
    static QString getNumericName(
        QnResourcePool* resPool,
        const QnVirtualCameraResourceList& devices,
        bool capitalize = true);

    /**
     * @brief Calculate common name for the target devices list.
     * @details Following rules are applied:
     * * If all devices are cameras - "%n Cameras";
     * * If all devices are I/O Modules - "%n I/O Modules";
     * * For mixed list "%n Devices" is used;
     * @param capitalize Should the word begin from the capital letter, default value is true.
     */
    static QString getNumericName(
        QnCameraDeviceType deviceType,
        int count,
        bool capitalize = true);

    /**
     * @brief Select string from the given set based on the target devices list.
     */
    static QString getNameFromSet(
        QnResourcePool* resPool,
        const QnCameraDeviceStringSet& set,
        const QnVirtualCameraResourceList& devices);

    /**
     * @brief Select string from the given set based on the target devices list.
     */
    static QString getNameFromSet(
        QnResourcePool* resPool,
        const QnCameraDeviceStringSet& set,
        const QnVirtualCameraResourcePtr& device);

    /**
     * @brief Select default string from the given set based on all devices in the system.
     */
    static QString getDefaultNameFromSet(
        QnResourcePool* resPool,
        const QnCameraDeviceStringSet& set);

    /**
     * @brief Select default string from the given set based on all devices in the system.
     */
    static QString getDefaultNameFromSet(
        QnResourcePool* resPool,
        const QString& mixedString,
        const QString& cameraString);

    /**
     * @brief Calculate device type based on the target devices list.
     */
    static QnCameraDeviceType calculateDeviceType(
        const QnResourcePool* resPool,
        const QnVirtualCameraResourceList& devices);
};
