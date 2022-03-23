// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_dependent_strings.h"

#include <algorithm>

#include <QtCore/QCoreApplication>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace
{

class QnResourceNameStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnResourceNameStrings)

public:
    static QString numericCameras(int count, bool capitalize)
    {
        return capitalize ? tr("%n Cameras", "", count) : tr("%n cameras", "", count);
    }

    static QString numericIoModules(int count, bool capitalize)
    {
        return capitalize ? tr("%n I/O Modules", "", count) : tr("%n I/O modules", "", count);
    }

    static QString numericDevices(int count, bool capitalize)
    {
        return capitalize ? tr("%n Devices", "", count) : tr("%n devices", "", count);
    }

    static QString defaultCameras(bool plural, bool capitalize)
    {
        if (plural)
            return capitalize ? tr("Cameras") : tr("cameras");
        return capitalize ? tr("Camera") : tr("camera");
    }

    static QString defaultIoModules(bool plural, bool capitalize)
    {
        if (plural)
            return capitalize ? tr("I/O Modules") : tr("I/O modules");
        return capitalize ? tr("I/O Module") : tr("I/O module");
    }

    static QString defaultDevices(bool plural, bool capitalize)
    {
        if (plural)
            return capitalize ? tr("Devices") : tr("devices");
        return capitalize ? tr("Device") : tr("device");
    }
};

QnCameraDeviceType calculateDefaultDeviceType(
    const QnResourcePool* resPool)
{
    return resPool && resPool->containsIoModules()
        ? QnCameraDeviceType::Mixed
        : QnCameraDeviceType::Camera;
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnCameraDeviceStringSet

QnCameraDeviceStringSet::QnCameraDeviceStringSet()
{
}

QnCameraDeviceStringSet::QnCameraDeviceStringSet(
    const QString& mixedString, const QString& cameraString, const QString& ioModuleString)
{
    setString(Mixed, true, mixedString);
    setString(Mixed, false, mixedString);

    setString(Camera, true, cameraString);
    setString(Camera, false, cameraString);

    // I/O Module string can be absent in cases where we are selecting default names.
    setString(IOModule, true, ioModuleString.isEmpty() ? mixedString : ioModuleString);
    setString(IOModule, false, ioModuleString.isEmpty() ? mixedString : ioModuleString);

    NX_ASSERT(isValid(), "Invalid string set");
}

QnCameraDeviceStringSet::QnCameraDeviceStringSet(
    const QString& mixedSigularString,
    const QString& mixedPluralString,
    const QString& cameraSigularString,
    const QString& cameraPluralString,
    const QString& ioModuleSigularString,
    const QString& ioModulePluralString)
{
    setString(Mixed, false, mixedSigularString);
    setString(Mixed, true, mixedPluralString);
    setString(Camera, false, cameraSigularString);
    setString(Camera, true, cameraPluralString);
    setString(IOModule, false, ioModuleSigularString);
    setString(IOModule, true, ioModulePluralString);

    NX_ASSERT(isValid(), "Invalid string set");
}

QString QnCameraDeviceStringSet::getString(QnCameraDeviceType deviceType, bool plural) const
{
    NX_ASSERT(isValid(), "Invalid string set");
    NX_ASSERT(deviceType < QnCameraDeviceType::Count, "Check if device type is valid");

    if (deviceType >= QnCameraDeviceType::Count)
        deviceType = QnCameraDeviceType::Mixed;

    return plural ? m_pluralStrings[deviceType] : m_singularStrings[deviceType];
}

void QnCameraDeviceStringSet::setString(
    QnCameraDeviceType deviceType, bool plural, const QString& value)
{
    if (plural)
        m_pluralStrings[deviceType] = value;
    else
        m_singularStrings[deviceType] = value;
}

bool QnCameraDeviceStringSet::isValid() const
{
    const auto stringIsValid =
        [](const QString& value) { return !value.isEmpty(); };

    return std::all_of(m_singularStrings.cbegin(), m_singularStrings.cend(), stringIsValid)
        && std::all_of(m_pluralStrings.cbegin(), m_pluralStrings.cend(), stringIsValid);
}

//-------------------------------------------------------------------------------------------------
// QnDeviceDependentStrings

QString QnDeviceDependentStrings::getNumericName(
    QnResourcePool* resPool,
    const QnVirtualCameraResourceList& devices,
    bool capitalize /*= true*/)
{
    return getNumericName(calculateDeviceType(resPool, devices), devices.size(), capitalize);
}

QString QnDeviceDependentStrings::getNumericName(
    QnCameraDeviceType deviceType,
    int count,
    bool capitalize)
{
    switch (deviceType)
    {
        case Camera:
            return QnResourceNameStrings::numericCameras(count, capitalize);
        case IOModule:
            return QnResourceNameStrings::numericIoModules(count, capitalize);
        case Mixed:
            return QnResourceNameStrings::numericDevices(count, capitalize);
        default:
            NX_ASSERT(false, "All fixed device types should be handled");
            return QnResourceNameStrings::numericDevices(count, capitalize);
    }
}

QString QnDeviceDependentStrings::getNameFromSet(
    QnResourcePool* resPool,
    const QnCameraDeviceStringSet& set,
    const QnVirtualCameraResourceList& devices)
{
    return set.getString(calculateDeviceType(resPool, devices), devices.size() != 1);
}

QString QnDeviceDependentStrings::getNameFromSet(
    QnResourcePool* resPool,
    const QnCameraDeviceStringSet& set,
    const QnVirtualCameraResourcePtr& device)
{
    const auto deviceType = device
        ? calculateDeviceType(resPool, {device})
        : calculateDefaultDeviceType(resPool);

    return set.getString(deviceType, false);
}

QString QnDeviceDependentStrings::getDefaultNameFromSet(
    QnResourcePool* resPool,
    const QnCameraDeviceStringSet& set)
{
    return set.getString(calculateDefaultDeviceType(resPool), true);
}

QString QnDeviceDependentStrings::getDefaultNameFromSet(
    QnResourcePool* resPool,
    const QString& mixedString,
    const QString& cameraString)
{
    return QnCameraDeviceStringSet(mixedString, cameraString).getString(
        calculateDefaultDeviceType(resPool), true);
}

QnCameraDeviceType QnDeviceDependentStrings::calculateDeviceType(
    const QnResourcePool* resPool,
    const QnVirtualCameraResourceList& devices)
{
    // Quick check whether there's no i/o modules in the system at all.
    if (!resPool || !resPool->containsIoModules())
        return QnCameraDeviceType::Camera;

    const bool hasCameras = std::any_of(devices.begin(), devices.end(),
        [](const auto& device) { return device->hasVideo(); });

    const bool hasIoModules = std::any_of(devices.begin(), devices.end(),
        [](const auto& device) { return device->isIOModule(); });

    // If only one type of devices.
    if (hasCameras ^ hasIoModules)
    {
        if (hasCameras)
            return QnCameraDeviceType::Camera;
        if (hasIoModules)
            return QnCameraDeviceType::IOModule;
    }

    // Either both of them or none.
    return QnCameraDeviceType::Mixed;
}
