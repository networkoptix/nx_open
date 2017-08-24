#include "mime_data.h"

#include <QtCore/QFile>

#include <QtWidgets/QApplication>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/file_processor.h>

namespace {

enum
{
    RESOURCES_BINARY_V1_TAG = 0xE1E00001,
    RESOURCES_BINARY_V2_TAG = 0xE1E00002,
};

Q_GLOBAL_STATIC_WITH_ARGS(quint64, qn_localMagic, (QDateTime::currentMSecsSinceEpoch()));

static const QString kInternalMimeType = lit("application/x-noptix-resources");
static const QString kUriListMimeType = lit("text/uri-list"); //< Must be equal to Qt internal type

QByteArray serializeToInternal(const QList<QnUuid>& ids)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    // Magic signature.
    stream << static_cast<quint32>(RESOURCES_BINARY_V2_TAG);

    // For local D&D.
    stream << static_cast<quint64>(QApplication::applicationPid());
    stream << static_cast<quint64>(*qn_localMagic());

    // Data.
    stream << static_cast<quint32>(ids.size());
    for (const auto& id: ids)
        stream << id.toString();

    return result;
}

QList<QnUuid> deserializeFromInternal(const QByteArray& data)
{
    QByteArray tmp = data;
    QDataStream stream(&tmp, QIODevice::ReadOnly);
    QList<QnUuid> result;

    quint32 tag;
    stream >> tag;
    if (tag != RESOURCES_BINARY_V1_TAG && tag != RESOURCES_BINARY_V2_TAG)
        return result;

    bool fromOtherApp = false;

    quint64 pid;
    stream >> pid;
    if (pid != (quint64)QApplication::applicationPid())
        fromOtherApp = true;

    quint64 magic;
    stream >> magic;
    if (magic != *qn_localMagic())
        fromOtherApp = true;

    quint32 size;
    stream >> size;
    for (quint32 i = 0; i < size; i++)
    {
        QString id;
        stream >> id;
        result.push_back(QnUuid(id));

        if (tag == RESOURCES_BINARY_V1_TAG)
        {
            QString uniqueId; //< Is not used in the later format.
            stream >> uniqueId;
        }

        if (stream.status() != QDataStream::Ok)
            break;
    }

    return result;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

MimeData::MimeData()
{
}

MimeData::MimeData(const QMimeData* data, QnResourcePool* resourcePool)
{
    load(data, resourcePool);
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
    return m_data.value(mimeType);
}

void MimeData::setData(const QString& mimeType, const QByteArray& data)
{
    m_data[mimeType] = data;
}

QStringList MimeData::formats() const
{
    return m_data.keys();
}

bool MimeData::hasFormat(const QString& mimeType) const
{
    return m_data.contains(mimeType);
}

void MimeData::removeFormat(const QString& mimeType)
{
    m_data.remove(mimeType);
}

QnResourceList MimeData::resources() const
{
    return m_resources;
}

void MimeData::setResources(const QnResourceList& resources)
{
    m_resources = resources.filtered(QnResourceAccessFilter::isDroppable);
    updateInternalStorage();
}

QList<QnUuid> MimeData::entities() const
{
    return m_entities;
}

void MimeData::setEntities(const QList<QnUuid>& ids)
{
    m_entities = ids;
    updateInternalStorage();
}

QString MimeData::serialized() const
{
    QByteArray serializedData;
    QDataStream stream(&serializedData, QIODevice::WriteOnly);
    stream << m_data;
    return QLatin1String(serializedData.toBase64());
}

MimeData MimeData::deserialized(QByteArray data, QnResourcePool* resourcePool)
{
    QDataStream stream(&data, QIODevice::ReadOnly);
    MimeData result;
    stream >> result.m_data;
    if (stream.status() != QDataStream::Ok || result.formats().empty())
        return result;

    // Code reuse...
    QMimeData mimeData;
    result.toMimeData(&mimeData);
    result.load(&mimeData, resourcePool);
    return result;
}

bool MimeData::isEmpty() const
{
    return m_entities.empty() && m_resources.empty();
}

void MimeData::load(const QMimeData* data, QnResourcePool* resourcePool)
{
    for (const QString &format: data->formats())
        setData(format, data->data(format));

    auto ids = deserializeFromInternal(this->data(kInternalMimeType));

    m_resources.clear();

    // Intentionally leave duplicates here to keep Ctrl+Drag behavior consistent.
    if (resourcePool)
        m_resources = resourcePool->getResources(ids);

    if (data->hasUrls())
    {
        auto urlResources = QnFileProcessor::findOrCreateResourcesForFiles(data->urls(),
            resourcePool).toSet();

        // Remove duplicates because resources can have both id and url.
        for (const auto& resource: m_resources)
            urlResources.remove(resource);

        m_resources.append(urlResources.toList());
    }

    for (const auto resource: m_resources)
        ids.removeAll(resource->getId());
    m_entities = ids;
    m_resources = m_resources.filtered(QnResourceAccessFilter::isDroppable);

    updateInternalStorage();
}

void MimeData::updateInternalStorage()
{
    QList<QnUuid> ids = m_entities;
    QList<QUrl> urls;
    for (const auto& resource: m_resources)
    {
        ids.append(resource->getId());
        if (resource->hasFlags(Qn::url | Qn::local))
            urls.append(QUrl::fromLocalFile(resource->getUrl()));
    }

    setData(kInternalMimeType, serializeToInternal(ids));

    if (!urls.empty())
    {
        QMimeData d;
        d.setUrls(urls);
        setData(kUriListMimeType, d.data(kUriListMimeType));
    }
}

} // namespace desktop
} // namespace client
} // namespace nx