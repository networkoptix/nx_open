#include "device_dependent_strings.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace {

    class QnResourceNameStrings {
        Q_DECLARE_TR_FUNCTIONS(QnResourceNameStrings)
    public:
        static QString numericCameras(int count, bool capitalize) {
            return capitalize       ? tr("%n Camera(s)", 0, count)      : tr("%n camera(s)", 0, count);
        }

        static QString numericIoModules(int count, bool capitalize) {
            return capitalize       ? tr("%n I/O Module(s)", 0, count)  : tr("%n I/O module(s)", 0, count);
        }

        static QString numericDevices(int count, bool capitalize) {
            return capitalize       ? tr("%n Device(s)", 0, count)      : tr("%n device(s)", 0, count);
        }

        static QString defaultCameras(bool plural, bool capitalize) {
            if (plural)
                return capitalize   ? tr("Cameras")                     : tr("cameras");
            return capitalize       ? tr("Camera")                      : tr("camera");
        }

        static QString defaultIoModules(bool plural, bool capitalize) {
            if (plural)
                return capitalize   ? tr("I/O Modules")                 : tr("I/O modules");
            return capitalize       ? tr("I/O Module")                  : tr("I/O module");
        }

        static QString defaultDevices(bool plural, bool capitalize) {
            if (plural)
                return capitalize   ? tr("Devices")                     : tr("devices");
            return capitalize       ? tr("Device")                      : tr("device");
        }

    };


    QnCameraDeviceType calculateDeviceType(
        const QnResourcePool* resPool,
        const QnVirtualCameraResourceList &devices)
    {
        /* Quick check - if there are no i/o modules in the system at all. */
        if (!resPool || !resPool->containsIoModules())
            return QnCameraDeviceType::Camera;

        using boost::algorithm::any_of;

        bool hasCameras = any_of(devices, [](const QnVirtualCameraResourcePtr &device) {
            return device->hasVideo(nullptr);
        });

        bool hasIoModules = any_of(devices, [](const QnVirtualCameraResourcePtr &device) {
            return device->isIOModule() && !device->hasVideo(nullptr);
        });

        /* Only one type of devices. */
        if (hasCameras ^ hasIoModules) {
            if (hasCameras)
                return QnCameraDeviceType::Camera;
            if (hasIoModules)
                return QnCameraDeviceType::IOModule;
        }

        /* Both of them - or none. */
        return QnCameraDeviceType::Mixed;
    }

    QnCameraDeviceType calculateDefaultDeviceType(const QnResourcePool* resPool)
    {
        if (!resPool || !resPool->containsIoModules())
            return QnCameraDeviceType::Camera;
        return QnCameraDeviceType::Mixed;
    }

}

QnCameraDeviceStringSet::QnCameraDeviceStringSet() {
}

QnCameraDeviceStringSet::QnCameraDeviceStringSet(const QString &mixedString, const QString &cameraString, const QString &ioModuleString) {
    setString(Mixed, true, mixedString);
    setString(Mixed, false, mixedString);

    setString(Camera, true, cameraString);
    setString(Camera, false, cameraString);

    /* I/O Module string can be absent in cases where we are selecting default names. */
    setString(IOModule, true, ioModuleString.isEmpty() ? lit("<invalid>") : ioModuleString);
    setString(IOModule, false, ioModuleString.isEmpty() ? lit("<invalid>") : ioModuleString);

    NX_ASSERT(isValid(), Q_FUNC_INFO, "Invalid string set");
}

QnCameraDeviceStringSet::QnCameraDeviceStringSet(
      const QString &mixedSigularString
    , const QString &mixedPluralString
    , const QString &cameraSigularString
    , const QString &cameraPluralString
    , const QString &ioModuleSigularString
    , const QString &ioModulePluralString)
{
    setString(Mixed,    false,  mixedSigularString);
    setString(Mixed,    true,   mixedPluralString);
    setString(Camera,   false,  cameraSigularString);
    setString(Camera,   true,   cameraPluralString);
    setString(IOModule, false,  ioModuleSigularString);
    setString(IOModule, true,   ioModulePluralString);

    NX_ASSERT(isValid(), Q_FUNC_INFO, "Invalid string set");
}

QString QnCameraDeviceStringSet::getString(QnCameraDeviceType deviceType, bool plural) const {
    NX_ASSERT(isValid(), Q_FUNC_INFO, "Invalid string set");
    NX_ASSERT(deviceType < QnCameraDeviceType::Count, Q_FUNC_INFO, "Check if device type is valid");
    if (deviceType >= QnCameraDeviceType::Count)
        deviceType = QnCameraDeviceType::Mixed;
    return plural
        ? m_pluralStrings[deviceType]
        : m_singularStrings[deviceType];
}

void QnCameraDeviceStringSet::setString(QnCameraDeviceType deviceType, bool plural, const QString &value) {
    if (plural)
        m_pluralStrings[deviceType] = value;
    else
        m_singularStrings[deviceType] = value;
}

bool QnCameraDeviceStringSet::isValid() const {
    auto stringIsValid = [](const QString &value) {
        return !value.isEmpty();
    };

    return std::all_of(m_singularStrings.cbegin(), m_singularStrings.cend(), stringIsValid)
        && std::all_of(m_pluralStrings.cbegin(), m_pluralStrings.cend(), stringIsValid);
}

/************************************************************************/
/* QnDeviceDependentStrings                                             */
/************************************************************************/
QString QnDeviceDependentStrings::getNumericName(
    QnResourcePool* resPool,
    const QnVirtualCameraResourceList &devices,
    bool capitalize /*= true*/)
{
    QnCameraDeviceType deviceType = calculateDeviceType(resPool, devices);
    switch (deviceType) {
    case Camera:
        return QnResourceNameStrings::numericCameras(devices.size(), capitalize);
    case IOModule:
        return QnResourceNameStrings::numericIoModules(devices.size(), capitalize);
    default:
        break;
    }
    NX_ASSERT(deviceType == Mixed, Q_FUNC_INFO, "All fixed device types should be handled");
    return QnResourceNameStrings::numericDevices(devices.size(), capitalize);
}

QString QnDeviceDependentStrings::getNameFromSet(
    QnResourcePool* resPool,
    const QnCameraDeviceStringSet &set,
    const QnVirtualCameraResourceList &devices)
{
    return set.getString(calculateDeviceType(resPool, devices), devices.size() != 1);
}

QString QnDeviceDependentStrings::getNameFromSet(
    QnResourcePool* resPool,
    const QnCameraDeviceStringSet &set,
    const QnVirtualCameraResourcePtr &device)
{
    return set.getString(
        device
        ? calculateDeviceType(resPool, QnVirtualCameraResourceList() << device)
        : calculateDefaultDeviceType(resPool)
    , false);
}

QString QnDeviceDependentStrings::getDefaultNameFromSet(
    QnResourcePool* resPool,
    const QnCameraDeviceStringSet &set)
{
    return set.getString(calculateDefaultDeviceType(resPool), true);
}

QString QnDeviceDependentStrings::getDefaultNameFromSet(
    QnResourcePool* resPool,
    const QString &mixedString,
    const QString &cameraString)
{
    return QnCameraDeviceStringSet(mixedString, cameraString).getString(calculateDefaultDeviceType(resPool), true);
}
