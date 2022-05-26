// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_data_pool.h"

#include <QtCore/QFile>

#include <nx/fusion/serialization/json_functions.h>
#include <core/resource/camera_resource.h>
#include <device_vendor_names/device_vendor_names.h>
#include "nx/utils/match/wildcard.h"

namespace {

static const QString delimiter = "|";
static const QString anyValue = "*";
static const QString tagVersion = "version";
static const QString tagKeys = "keys";
static const QString tagData = "data";

} // namespace

struct QnResourceDataPoolChunk
{
    QList<QString> keys;
    QnResourceData data;
};

static bool deserialize(
    QnJsonContext* ctx,
    const QJsonValue& value,
    QnResourceDataPoolChunk* target)
{
    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    QnResourceDataPoolChunk result;
    if(!QJson::deserialize(ctx, map, tagKeys, &result.keys)
       || !QJson::deserialize(ctx, value, &result.data))
    {
        return false;
    }

    result.data.clearKeyTags();

    *target = result;
    return true;
}

nx::utils::SoftwareVersion QnResourceDataPool::getVersion(const QByteArray& data)
{
    QJsonObject map;
    if (!QJson::deserialize(data, &map))
        return nx::utils::SoftwareVersion();
    QString version;
    if (!QJson::deserialize(map, tagVersion, &version))
        return nx::utils::SoftwareVersion();
    return nx::utils::SoftwareVersion(version);
}

static bool validateDataInternal(
    const QByteArray& data,
    QJsonObject& map,
    QList<QnResourceDataPoolChunk>& chunks)
{
    if (data.isEmpty())
        return false; /* Read error. */

    if (!QJson::deserialize(data, &map))
        return false;

    QString version;
    if (!QJson::deserialize(map, tagVersion, &version))
        return false;

    if (!QJson::deserialize(map, tagData, &chunks))
        return false;

    return true;
}

QnResourceDataPool::QnResourceDataPool(QObject* parent):
    QObject(parent)
{
}

QnResourceDataPool::~QnResourceDataPool()
{
}

QnResourceData QnResourceDataPool::data(const QnConstSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return QnResourceData();
    return data(camera->getVendor(), camera->getModel(), camera->getFirmware());
}

QnResourceData QnResourceDataPool::data(
    const QString& vendor,
    const QString& model,
    const QString& firmware) const
{
    return data(Key(vendor, model, firmware));
}

QnResourceData QnResourceDataPool::data(const Key& key) const
{
    QnResourceData result;
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto it = m_cachedResultByKey.find(key); it != m_cachedResultByKey.end())
        return it.value();

    for (const auto& [dataKey, data] : m_dataByKey)
    {
        if (dataKey.matches(key))
            result.add(data);
    }
    m_cachedResultByKey.insert(key, result);
    return result;
}

bool QnResourceDataPool::loadFile(const QString& fileName)
{
    if(!QFile::exists(fileName))
    {
        NX_ASSERT(false, "File '%1' does not exist", fileName);
        return false;
    }

    if(!loadInternal(fileName))
    {
        NX_ASSERT(false, "Error while loading resource data from file '%1'.", fileName);
        return false;
    }

    return true;
}

bool QnResourceDataPool::loadInternal(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 size = file.size();
    if(size == 0 || size > 16 * 1024 * 1024)
        return false; /* File is larger than 16Mb => definitely not JSON we need. */
    return loadData(file.readAll());
}

bool QnResourceDataPool::validateData(const QByteArray& data)
{
    QJsonObject map;
    QList<QnResourceDataPoolChunk> chunks;
    return validateDataInternal(data, map, chunks);
}

bool QnResourceDataPool::loadData(const QByteArray& data)
{
    QJsonObject map;
    QList<QnResourceDataPoolChunk> chunks;

    if(!validateDataInternal(data, map, chunks))
        return false;

    // merge data with same keys to improve runtime performance
    decltype(m_dataByKey) dataByKey;
    std::map<Key, std::size_t> indexes;
    for(const auto& chunk: std::as_const(chunks))
    {
        for(const auto& rawKey: chunk.keys)
        {
            auto key = Key::fromString(rawKey);
            const auto [it, isNew] = indexes.emplace(key, indexes.size());
            if (isNew)
                dataByKey.emplace_back(std::move(key), chunk.data);
            else
                dataByKey[it->second].second.add(chunk.data);
        }
    }

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_dataByKey = std::move(dataByKey);
        m_cachedResultByKey.clear();
    }

    m_allData = std::move(map);
    emit changed();
    return true;
}

void QnResourceDataPool::clear()
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_cachedResultByKey.clear();
        m_dataByKey.clear();
        m_allData = QJsonObject();
    }
    emit changed();
}

QJsonObject QnResourceDataPool::allData() const
{
    return m_allData;
}

QnResourceDataPool::Key::Key(
    const QString& _vendor,
    const QString& _model,
    const QString& _firmware)
    :
    vendor(shortVendorByName(_vendor.toLower())),
    model(_model.toLower()),
    firmware(_firmware.toLower())
{
}

QnResourceDataPool::Key QnResourceDataPool::Key::fromString(const QString& key)
{
    auto parts = key.split(delimiter);
    while (parts.size() < 3)
        parts.append(anyValue);
    return Key(parts[0], parts[1], parts[2]);
}

bool QnResourceDataPool::Key::matches(const Key& other) const
{
    return
        wildcardMatch(vendor, other.vendor) &&
        wildcardMatch(model, other.model) &&
        wildcardMatch(firmware, other.firmware);
}

bool QnResourceDataPool::Key::operator==(const Key& other) const
{
    return toComparable() == other.toComparable();
}

bool QnResourceDataPool::Key::operator<(const Key& other) const
{
    return toComparable() < other.toComparable();
}
