#include "resource_name.h"

#include <QHostAddress>

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
            return capitalize       ? tr("%n IO Module(s)", 0, count)   : tr("%n IO module(s)", 0, count); 
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
                return capitalize   ? tr("IO Modules")                  : tr("IO modules"); 
            return capitalize       ? tr("IO Module")                   : tr("IO module"); 
        }

        static QString defaultDevices(bool plural, bool capitalize) { 
            if (plural)
                return capitalize   ? tr("Devices")                     : tr("devices"); 
            return capitalize       ? tr("Device")                      : tr("device");
        }

    };

}

QString extractHost(const QString &url) 
{
    /* Don't go through QHostAddress/QUrl constructors as it is SLOW. 
     * Speed is important for event log. */ 
    int startPos = url.indexOf(lit("://"));
    startPos = startPos == -1 ? 0 : startPos + 3;

    int endPos = url.indexOf(L':', startPos);
    if (endPos == -1)
        endPos = url.indexOf(L'/', startPos); /* No port, but we may still get '/' after address. */
    endPos = endPos == -1 ? url.size() : endPos;

    return url.mid(startPos, endPos - startPos);
}

QString getFullResourceName(const QnResourcePtr &resource, bool showIp) {
    if (!resource)
        return QString();

    QString baseName = resource->getName();
    Qn::ResourceFlags flags = resource->flags();
    if (showIp && ((flags & Qn::network) || (flags & Qn::server && flags & Qn::remote))) {
        QString host = extractHost(resource->getUrl());
        if(!host.isEmpty())
            return QString(QLatin1String("%1 (%2)")).arg(baseName).arg(host);
    }
    return baseName;
}

QString getDefaultDevicesName(bool plural /*= true*/, bool capitalize /*= true*/) {
    if (!qnResPool->containsIoModules())
        return QnResourceNameStrings::defaultCameras(plural, capitalize);
    return QnResourceNameStrings::defaultDevices(plural, capitalize);
}

QString getDefaultDevicesName(const QnVirtualCameraResourceList &devices, bool capitalize /*= true*/) {
    bool plural = devices.size() != 1;

    /* Quick check - if there are no io modules in the system at all. */
    if (!qnResPool->containsIoModules())
        return QnResourceNameStrings::defaultCameras(plural, capitalize);

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
            return QnResourceNameStrings::defaultCameras(plural, capitalize);
        if (hasIoModules)
            return QnResourceNameStrings::defaultIoModules(plural, capitalize);
    }

    /* Both of them - or none. */
    return QnResourceNameStrings::defaultDevices(plural, capitalize);
}

QString getNumericDevicesName(const QnVirtualCameraResourceList &devices, bool capitalize /*= true*/) {
    /* Quick check - if there are no io modules in the system at all. */
    if (!qnResPool->containsIoModules())
        return QnResourceNameStrings::numericCameras(devices.size(), capitalize);

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
            return QnResourceNameStrings::numericCameras(devices.size(), capitalize);
        if (hasIoModules)
            return QnResourceNameStrings::numericIoModules(devices.size(), capitalize);
    }

    /* Both of them - or none. */
    return QnResourceNameStrings::numericDevices(devices.size(), capitalize);
}

QString getDefaultDeviceNameLower(const QnVirtualCameraResourcePtr &device /*= QnVirtualCameraResourcePtr()*/)
{
    if (device)
        return getDefaultDevicesName(QnVirtualCameraResourceList() << device, false);
    return getDefaultDevicesName(false, false);
}

QString getDefaultDeviceNameUpper(const QnVirtualCameraResourcePtr &device /*= QnVirtualCameraResourcePtr()*/)
{
    if (device)
        return getDefaultDevicesName(QnVirtualCameraResourceList() << device, true);
    return getDefaultDevicesName(false, true);
}
