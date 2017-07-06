#include "mime_data.h"

#include <QtWidgets/QApplication>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>

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

QList<QUrl> serializeResourcesToUriList(const QnResourceList& resources)
{
    QList<QUrl> result;
    for (const QnResourcePtr &resource : resources)
    {
        if (resource->hasFlags(Qn::url | Qn::local))
            result.append(QUrl::fromLocalFile(resource->getUrl()));
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

MimeData::MimeData(const QMimeData* data)
{
    load(data);
}

QStringList MimeData::mimeTypes()
{
    return {kInternalMimeType, kUriListMimeType};
}

void MimeData::toMimeData(QMimeData* data) const
{
    for (const QString &format : data->formats())
        data->removeFormat(format);

    for (const QString &format : this->formats())
        data->setData(format, this->data(format));
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

void MimeData::setResources(const QnResourceList& resources)
{
    QList<QnUuid> ids;
    QList<QUrl> urls;
    for (const auto& resource: resources)
    {
        ids << resource->getId();
        if (resource->hasFlags(Qn::url | Qn::local))
            urls.append(QUrl::fromLocalFile(resource->getUrl()));
    }
    setIds(ids);

    if (!urls.empty())
    {
        QMimeData d;
        d.setUrls(urls);
        load(&d);
    }
}

QList<QnUuid> MimeData::getIds() const
{
    return deserializeFromInternal(data(kInternalMimeType));
}

void MimeData::setIds(const QList<QnUuid>& ids)
{
    if (!ids.empty())
        setData(kInternalMimeType, serializeToInternal(ids));
}

QList<QUrl> MimeData::getUrls() const
{
    QMimeData mimeData;
    toMimeData(&mimeData);
    return mimeData.urls();
}

QString MimeData::serialized() const
{
    QByteArray serializedData;
    QDataStream stream(&serializedData, QIODevice::WriteOnly);
    stream << m_data;
    return QLatin1String(serializedData.toBase64());
}

void MimeData::load(const QMimeData* data)
{
    for (const QString &format : data->formats())
        setData(format, data->data(format));
}

QDataStream& operator<<(QDataStream& stream, const MimeData& data)
{
    return stream << data.m_data;
}

QDataStream& operator>> (QDataStream& stream, MimeData& data)
{
    return stream >> data.m_data;
}

} // namespace desktop
} // namespace client
} // namespace nx