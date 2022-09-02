// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mime_data.h"

#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtWidgets/QApplication>

#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/utils/app_info.h>
#include <nx/utils/qset.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>

namespace {

enum class Protocol
{
    unknown = -1,
    v40,
    v51,
};

enum
{
    RESOURCES_BINARY_V40_TAG = 0xE1E00003,
    RESOURCES_BINARY_V51_TAG = 0xE1E00004,
};

Protocol protocolFromTag(quint32 tag)
{
    switch (tag)
    {
        case RESOURCES_BINARY_V40_TAG:
            return Protocol::v40;
        case RESOURCES_BINARY_V51_TAG:
            return Protocol::v51;
        default:
            return Protocol::unknown;
    }
}

Q_GLOBAL_STATIC_WITH_ARGS(quint64, qn_localMagic, (QDateTime::currentMSecsSinceEpoch()));

static const QString kInternalMimeType = "application/x-noptix-resources";
static const QString kUriListMimeType = "text/uri-list"; //< Must be equal to Qt internal type.

} // namespace

namespace nx::vms::client::desktop {

struct MimeData::Private
{
    QHash<QString, QByteArray> data;
    QList<QnUuid> entities;
    QnResourceList resources;
    QHash<int, QVariant> arguments;

    /** Deserialize from the raw data.*/
    bool deserialize(QByteArray data, const QList<QUrl>& urls)
    {
        entities.clear();
        resources.clear();
        arguments.clear();

        QDataStream stream(&data, QIODevice::ReadOnly);

        quint32 tag;
        stream >> tag;

        const auto protocol = protocolFromTag(tag);
        if (protocol == Protocol::unknown)
            return deserializeUrls(urls);

        switch (protocol)
        {
            case Protocol::v40:
                return deserializeContentV4(stream, urls);
            case Protocol::v51:
                return deserializeContentV5(stream, urls);
            default:
                break;
        }
        return false;
    }

    bool deserializeUrls(const QList<QUrl>& urls)
    {
        SystemContext* systemContext = appContext()->currentSystemContext();
        if (!NX_ASSERT(systemContext))
            return false;

        auto resourcePool = systemContext->resourcePool();

        auto urlResources = nx::utils::toQSet(
            QnFileProcessor::findOrCreateResourcesForFiles(urls, resourcePool)
                .filtered(QnResourceAccessFilter::isDroppable));

        // Remove duplicates because resources can have both id and url.
        for (const auto& resource: resources)
            urlResources.remove(resource);

        resources.append(urlResources.values());

        for (const auto resource: urlResources)
            entities.removeAll(resource->getId());

        return !urls.empty();
    }

    /** Deserialize content using format from 4.0 version.*/
    bool deserializeContentV4(QDataStream& stream, const QList<QUrl>& urls)
    {
        quint64 pid;
        stream >> pid; //< Can be compared with QApplication::applicationPid() if needed.
        quint64 magic;
        stream >> magic; //< Can be compared with *qn_localMagic() if needed.

        // Resources and entities.
        SystemContext* systemContext = appContext()->currentSystemContext();
        if (!NX_ASSERT(systemContext))
            return false;

        auto resourcePool = systemContext->resourcePool();

        int idsCount = 0;
        stream >> idsCount;

        // Intentionally leave duplicates here to keep Ctrl+Drag behavior consistent.
        for (int i = 0; i < idsCount; i++)
        {
            QString sid;
            stream >> sid;
            const QnUuid id(sid);

            if (auto resource = resourcePool->getResourceById(id))
            {
                if (QnResourceAccessFilter::isDroppable(resource))
                    resources.push_back(resource);
            }
            else
            {
                entities.push_back(id);
            }
        }

        // Arguments.
        int argumentsCount = 0;
        stream >> argumentsCount;

        for (int i = 0; i < argumentsCount; i++)
        {
            int key = 0;
            QVariant value;
            stream >> key >> value;
            arguments[key] = value;
        }

        deserializeUrls(urls);
        return stream.status() == QDataStream::Ok;
    }

    /** Deserialize content using format from 5.1 version.*/
    bool deserializeContentV5(QDataStream& stream, const QList<QUrl>& urls)
    {
        // Resources.
        {
            int resourcesCount = 0;
            stream >> resourcesCount;

            // Intentionally leave duplicates here to keep Ctrl+Drag behavior consistent.
            for (int i = 0; i < resourcesCount; i++)
            {
                nx::vms::common::ResourceDescriptor descriptor;
                stream >> descriptor.id >> descriptor.path;

                if (auto resource = getResourceByDescriptor(descriptor);
                    resource && QnResourceAccessFilter::isDroppable(resource))
                {
                    resources.push_back(resource);
                }
            }
        }

        // Entities.
        {
            int entitiesCount = 0;
            for (int i = 0; i < entitiesCount; i++)
            {
                QnUuid id;
                stream >> id;
                entities.push_back(id);
            }
        }

        // Arguments.
        {
            int argumentsCount = 0;
            stream >> argumentsCount;

            for (int i = 0; i < argumentsCount; i++)
            {
                int key = 0;
                QVariant value;
                stream >> key >> value;
                arguments[key] = value;
            }
        }

        deserializeUrls(urls);
        return stream.status() == QDataStream::Ok;
    }

    /** Store serialized content into data storage. */
    void updateInternalStorage()
    {
        QByteArray serialized;
        QDataStream stream(&serialized, QIODevice::WriteOnly);

        // Magic signature.
        stream << static_cast<quint32>(RESOURCES_BINARY_V51_TAG);

        // Resources.
        stream << static_cast<quint32>(resources.size());
        for (const auto& resource: resources)
        {
            auto d = descriptor(resource);
            stream << d.id << d.path;
        }

        // Entities.
        stream << static_cast<quint32>(entities.size());
        for (const auto& entity: entities)
            stream << entity;

        // Arguments.
        stream << static_cast<quint32>(arguments.size());
        for (const auto& [key, value]: nx::utils::constKeyValueRange(arguments))
            stream << key << value;

        data[kInternalMimeType] = serialized;
    }
};

MimeData::MimeData():
    d(new Private())
{
}

MimeData::MimeData(const QMimeData* data):
    MimeData()
{
    load(data);
}

MimeData::~MimeData()
{
}

MimeData::MimeData(QByteArray data):
    MimeData()
{
    QDataStream stream(&data, QIODevice::ReadOnly);
    stream >> d->data;
    if (stream.status() != QDataStream::Ok || formats().empty())
        return;

    // Code reuse.
    QMimeData mimeData;
    toMimeData(&mimeData);
    load(&mimeData);
}

QStringList MimeData::mimeTypes()
{
    return {kInternalMimeType, kUriListMimeType};
}

void MimeData::toMimeData(QMimeData* data) const
{
    for (const QString& format: data->formats())
        data->removeFormat(format);

    for (const QString& format: this->formats())
        data->setData(format, this->data(format));
}

QMimeData* MimeData::createMimeData() const
{
    QMimeData* result = new QMimeData();
    toMimeData(result);
    return result;
}

QByteArray MimeData::data(const QString& mimeType) const
{
    return d->data.value(mimeType);
}

void MimeData::setData(const QString& mimeType, const QByteArray& data)
{
    d->data[mimeType] = data;
}

QStringList MimeData::formats() const
{
    return d->data.keys();
}

bool MimeData::hasFormat(const QString& mimeType) const
{
    return d->data.contains(mimeType);
}

void MimeData::removeFormat(const QString& mimeType)
{
    d->data.remove(mimeType);
}

QnResourceList MimeData::resources() const
{
    return d->resources;
}

void MimeData::setResources(const QnResourceList& resources)
{
    d->resources = resources.filtered(QnResourceAccessFilter::isDroppable);
    d->updateInternalStorage();
}

QList<QnUuid> MimeData::entities() const
{
    return d->entities;
}

void MimeData::setEntities(const QList<QnUuid>& ids)
{
    d->entities = ids;
    d->updateInternalStorage();
}

QString MimeData::serialized() const
{
    QByteArray serializedData;
    QDataStream stream(&serializedData, QIODevice::WriteOnly);
    stream << d->data;
    return QLatin1String(serializedData.toBase64());
}

bool MimeData::isEmpty() const
{
    return d->entities.empty() && d->resources.empty();
}

void MimeData::load(const QMimeData* data)
{
    for (const QString& format: data->formats())
        setData(format, data->data(format));

    QList<QUrl> urls;
    if (data->hasUrls())
        urls = data->urls();

    d->deserialize(this->data(kInternalMimeType), urls);
    d->updateInternalStorage();
}

QHash<int, QVariant> MimeData::arguments() const
{
    return d->arguments;
}

void MimeData::setArguments(const QHash<int, QVariant>& value)
{
    d->arguments = value;
    d->updateInternalStorage();
}

} // namespace nx::vms::client::desktop
