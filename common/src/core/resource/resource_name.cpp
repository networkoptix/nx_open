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
        static QString cameras(bool capitalize, int count) { 
            if (count == 0)
                return capitalize   ? tr("Cameras")                     : tr("cameras"); 
            if (count == 1)
                return capitalize   ? tr("Camera")                      : tr("camera"); 
            return capitalize       ? tr("%n Camera(s)", 0, count)      : tr("%n camera(s)", 0, count); 
        }

        static QString iomodules(bool capitalize, int count) {
            if (count == 0)
                return capitalize   ? tr("IO Modules")                  : tr("IO modules"); 
            if (count == 1)
                return capitalize   ? tr("IO Module")                   : tr("IO module");
            return capitalize       ? tr("%n IO Module(s)", 0, count)   : tr("%n IO module(s)", 0, count); 
        }

        static QString devices(bool capitalize, int count) { 
            if (count == 0)
                return capitalize   ? tr("Devices")                     : tr("devices"); 
            if (count == 1)
                return capitalize   ? tr("Device")                      : tr("device");
            return capitalize       ? tr("%n Device(s)", 0, count)      : tr("%n device(s)", 0, count); 
        }

        static QString selectCamera()   {   return tr("Please select at least one device."); }
        static QString selectDevice()   {   return tr("Please select at least one device."); }
        static QString anyCamera()      {   return tr("Any Camera"); }
        static QString anyDevice()      {   return tr("Any Device"); }

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

QString getDevicesName(const QnVirtualCameraResourceList &devices, bool capitalize /* = true*/) {

    /* Quick check - if there are no io modules in the system at all. */
    if (!qnResPool->containsIoModules())
        return QnResourceNameStrings::cameras(capitalize, devices.size());

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
            return QnResourceNameStrings::cameras(capitalize, devices.size());
        if (hasIoModules)
            return QnResourceNameStrings::iomodules(capitalize, devices.size());
    }

    /* Both of them - or none. */
    return QnResourceNameStrings::devices(capitalize, devices.size());
}

QString selectDevice() {
    /* Quick check - if there are no io modules in the system at all. */
    if (!qnResPool->containsIoModules())
        return QnResourceNameStrings::selectCamera();
    return QnResourceNameStrings::selectDevice();
}

QString anyDevice() {
    /* Quick check - if there are no io modules in the system at all. */
    if (!qnResPool->containsIoModules())
        return QnResourceNameStrings::anyCamera();
    return QnResourceNameStrings::anyDevice();
}
