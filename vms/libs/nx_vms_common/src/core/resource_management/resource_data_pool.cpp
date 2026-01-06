// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_data_pool.h"

#include <QtCore/QFile>

#include <core/resource/camera_resource.h>
#include <device_vendor_names/device_vendor_names.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/match/wildcard.h>

namespace {

static const QString delimiter = "|";
static const QString anyValue = "*";
static const QString tagVersion = "version";
static const QString tagKeys = "keys";
static const QString tagData = "data";

} // namespace

static bool deserialize(
    QnJsonContext* ctx,
    const QJsonValue& value,
    QnResourceDataPool::QnResourceDataPoolChunk* target)
{
    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    QnResourceDataPool::QnResourceDataPoolChunk result;
    if(!QJson::deserialize(ctx, map, tagKeys, &result.keys)
       || !QJson::deserialize(ctx, value, &result.data))
    {
        return false;
    }

    result.data.clearKeyTags();

    *target = result;
    return true;
}

QnResourceDataPool::DeserializeResult QnResourceDataPool::deserializeData(
    const QByteArray& data)
{
    if (data.isEmpty())
        return std::unexpected("Empty data");

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        return std::unexpected(error.errorString());

    ResourceDataPoolData result{.jsonData = document.object()};

    QString versionStr;
    if (!QJson::deserialize(result.jsonData, tagVersion, &versionStr))
        return std::unexpected("Unabled to deserialize version");
    result.version = nx::utils::SoftwareVersion(versionStr);

    if (!QJson::deserialize(result.jsonData, tagData, &result.chunks))
        return std::unexpected("Unabled to deserialize chunks");

    return result;
}

QnResourceDataPool::QnResourceDataPool(QObject* parent):
    QObject(parent)
{
}

QnResourceDataPool::~QnResourceDataPool()
{
}

QnResourceData QnResourceDataPool::data(
    const QString& vendor,
    const QString& model,
    const QString& firmware) const
{
    if (m_externalSource)
        return m_externalSource->data(vendor, model, firmware);

    return data(Key(vendor, model, firmware));
}

void QnResourceDataPool::setExternalSource(QnResourceDataPool* source)
{
    m_externalSource = source;
    if (source)
        connect(source, &QnResourceDataPool::changed, this, &QnResourceDataPool::changed);
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

void QnResourceDataPool::setData(
    QJsonObject allData,
    std::vector<std::pair<Key, QnResourceData>> dataByKey)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_dataByKey = std::move(dataByKey);
        m_allData = std::move(allData);
        m_cachedResultByKey.clear();
    }

    emit changed();
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

bool QnResourceDataPool::loadData(const QByteArray& data)
{
    auto result = deserializeData(data);

    if (!result.has_value())
        return false;

    decltype(m_dataByKey) dataByKey;
    for (const auto& chunk: result->chunks)
    {
        for (const auto& rawKey: chunk.keys)
            dataByKey.emplace_back(Key::fromString(rawKey), chunk.data);
    }

    setData(std::move(result->jsonData), std::move(dataByKey));
    return true;
}

void QnResourceDataPool::clear()
{
    setData({}, {});
}

QJsonObject QnResourceDataPool::allData() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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
