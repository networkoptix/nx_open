#include "workbench_resource.h"

#include <QtWidgets/QApplication>
#include <QtCore/QMimeData>

#include <utils/common/warnings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/file_processor.h>


namespace {
    enum {
        RESOURCES_BINARY_V1_TAG = 0xE1E00001,
    };

    Q_GLOBAL_STATIC_WITH_ARGS(quint64, qn_localMagic, (QDateTime::currentMSecsSinceEpoch()));

    const char *privateMimeType = "application/x-noptix-resources";
    const char *uriListMimeType = "text/uri-list";

    QByteArray serializeResourcesToPrivate(const QnResourceList &resources) {
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

    QList<QUrl> serializeResourcesToUriList(const QnResourceList &resources) {
        QList<QUrl> result;
        foreach (const QnResourcePtr &resource, resources) {
            if (resource->hasFlags(Qn::url))
                result.append(QUrl::fromLocalFile(resource->getUrl()));
        }
        return result;
    }

    QnResourceList deserializeResourcesFromPrivate(const QByteArray &data) {
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
        if(pid != (quint64)QApplication::applicationPid())
            fromOtherApp = true;

        quint64 magic;
        stream >> magic;
        if(magic != *qn_localMagic())
            fromOtherApp = true;

        quint32 size;
        stream >> size;
        for(unsigned i = 0; i < size; i++) {
            QString id, uniqueId;
            stream >> id >> uniqueId;

            QnResourcePtr resource = qnResPool->getResourceById(QUuid(id));
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

    QnResourceList deserializeResourcesFromUriList(const QList<QUrl> &urls) {
        return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(urls));
    }

} // anonymous namespace

QStringList QnWorkbenchResource::resourceMimeTypes() {
    QStringList result;
    result.push_back(QLatin1String(privateMimeType));
    result.push_back(QLatin1String(uriListMimeType));
    return result;
}

void QnWorkbenchResource::serializeResources(const QnResourceList &resources, const QStringList &mimeTypes, QMimeData *mimeData) {
    if(mimeData == NULL) {
        qnNullWarning(mimeData);
        return;
    }

    if(mimeTypes.contains(QLatin1String(privateMimeType)))
        mimeData->setData(QLatin1String(privateMimeType), serializeResourcesToPrivate(resources));

    if(mimeTypes.contains(QLatin1String(uriListMimeType)))
        mimeData->setUrls(serializeResourcesToUriList(resources));
}

QnResourceList QnWorkbenchResource::deserializeResources(const QMimeData *mimeData) {
    QnResourceList result;

    if (mimeData->hasFormat(QLatin1String(privateMimeType))) {
        result = deserializeResourcesFromPrivate(mimeData->data(QLatin1String(privateMimeType)));
    } else if (mimeData->hasUrls()) {
        result = deserializeResourcesFromUriList(mimeData->urls());
    }

    return result;
}
