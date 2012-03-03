#include "view_drag_and_drop.h"
#include <QApplication>
#include <core/resourcemanagment/resource_pool.h>
#include "utils/common/synctime.h"

namespace {
    enum {
        RESOURCES_BINARY_V1_TAG = 0xE1E00001
    };

    // do not access to app server time for static variables!
    // If access occured before appServer conection is initialized in main, connection is not work at all
    quint64 localMagic = QDateTime::currentMSecsSinceEpoch();

} // anonymous namespace

QString resourcesMime() {
    return QLatin1String("application/x-noptix-resources");
}

QByteArray serializeResources(const QnResourceList &resources) {
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    /* Magic signature. */
    stream << static_cast<quint32>(RESOURCES_BINARY_V1_TAG);
    
    /* For local D&D. */
    stream << static_cast<quint64>(QApplication::applicationPid());
    stream << static_cast<quint64>(localMagic);

    /* Data. */
    stream << static_cast<quint32>(resources.size());
    foreach(const QnResourcePtr &resource, resources)
        stream << resource->getId().toString();

    return result;
}

QnResourceList deserializeResources(const QByteArray &data) {
    QByteArray tmp = data;
    QDataStream stream(&tmp, QIODevice::ReadOnly);
    QnResourceList result;

    quint32 tag;
    stream >> tag;
    if(tag != RESOURCES_BINARY_V1_TAG)
        return result;

    quint64 pid;
    stream >> pid;
    if(pid != QApplication::applicationPid())
        return result; // TODO: originated from another application.

    quint64 magic;
    stream >> magic;
    if(magic != localMagic)
        return result; // TODO: originated from another application.

    quint32 size;
    stream >> size;
    for(int i = 0; i < size; i++) {
        QString id;
        stream >> id;

        QnResourcePtr resource = qnResPool->getResourceById(id);
        if(!resource.isNull())
            result.push_back(resource);

        if(stream.status() != QDataStream::Ok)
            break;
    }

    return result;
}
