#include "workbench_resource.h"
#include <QApplication>
#include <core/resourcemanagment/resource_pool.h>
#include "utils/common/synctime.h"
#include "file_processor.h"

namespace {
    enum {
        RESOURCES_BINARY_V1_TAG = 0xE1E00001,
    };

    Q_GLOBAL_STATIC_WITH_ARGS(quint64, qn_localMagic, (QDateTime::currentMSecsSinceEpoch()));

} // anonymous namespace

QString QnWorkbenchResource::resourcesMime() {
    return QLatin1String("application/x-noptix-resources");
}

QByteArray QnWorkbenchResource::serializeResources(const QnResourceList &resources) {
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    /* Magic signature. */
    stream << static_cast<quint32>(RESOURCES_BINARY_V1_TAG);

    /* For local D&D. */
    stream << static_cast<quint64>(QApplication::applicationPid());
    stream << static_cast<quint64>(*qn_localMagic());

    /* Data. */
    stream << static_cast<quint32>(resources.size());
    foreach(const QnResourcePtr &resource, resources)
        stream << resource->getId().toString() << resource->getUniqueId();

    return result;
}

QnResourceList QnWorkbenchResource::deserializeResources(const QByteArray &data) {
    QByteArray tmp = data;
    QDataStream stream(&tmp, QIODevice::ReadOnly);
    QnResourceList result;

    quint32 tag;
    stream >> tag;
    if(tag != RESOURCES_BINARY_V1_TAG)
        return result;

    bool fromOtherApp = false;

    quint64 pid;
    stream >> pid;
    if(pid != QApplication::applicationPid())
        fromOtherApp = true;

    quint64 magic;
    stream >> magic;
    if(magic != *qn_localMagic())
        fromOtherApp = true;

    quint32 size;
    stream >> size;
    for(int i = 0; i < size; i++) {
        QString id, uniqueId;
        stream >> id >> uniqueId;

        QnResourcePtr resource = qnResPool->getResourceById(id);
        if(resource && resource->getUniqueId() == uniqueId) {
            result.push_back(resource);
            continue;
        }

        if(fromOtherApp) {
            resource = qnResPool->getResourceByUniqId(uniqueId);
            if(resource) {
                result.push_back(resource);
                continue;
            }

            resource = QnFileProcessor::createResourcesForFile(uniqueId);
            if(resource) {
                result.push_back(resource);
                continue;
            }
        }

        if(stream.status() != QDataStream::Ok)
            break;
    }

    return result;
}

