#include "resource_data_pool.h"

#include <QtCore/QFile>

#include <utils/common/warnings.h>
#include <nx/fusion/serialization/json_functions.h>
#include <core/resource/camera_resource.h>
#include "nx/utils/match/wildcard.h"

struct QnResourceDataPoolChunk {
    QList<QString> keys;
    QnResourceData data;
};

static bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceDataPoolChunk *target) {
    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    QnResourceDataPoolChunk result;
    if(!QJson::deserialize(ctx, map, lit("keys"), &result.keys) || !QJson::deserialize(ctx, value, &result.data))
        return false;

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
    if (!QJson::deserialize(map, lit("version"), &version))
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
    if (!QJson::deserialize(map, lit("version"), &version))
        return false;

    if (!QJson::deserialize(map, lit("data"), &chunks))
        return false;

    return true;
}

QnResourceDataPool::QnResourceDataPool(QObject *parent):
    QObject(parent)
{
    // TODO: #dmishin move declaration of vendor short names
    // to resource_data.json or some other config.
    m_shortVendorByName.insert(lit("digital watchdog"), lit("dw"));
    m_shortVendorByName.insert(lit("digital_watchdog"), lit("dw"));
    m_shortVendorByName.insert(lit("digitalwatchdog"), lit("dw"));
    m_shortVendorByName.insert(lit("panoramic"), lit("dw"));
    m_shortVendorByName.insert(lit("ipnc"), lit("dw"));
    m_shortVendorByName.insert(lit("acti corporation"), lit("acti"));
    m_shortVendorByName.insert(lit("innovative security designs"), lit("isd"));
    m_shortVendorByName.insert(lit("norbain_"), lit("vista"));
    m_shortVendorByName.insert(lit("norbain"), lit("vista"));
    m_shortVendorByName.insert(lit("flir systems"), lit("flir"));
    m_shortVendorByName.insert(lit("hanwha techwin"), lit("hanwha"));
    m_shortVendorByName.insert(lit("samsung techwin"), lit("samsung"));
    m_shortVendorByName.insert(lit("2n telecommunications"), lit("2nt"));
    m_shortVendorByName.insert(lit("hangzhou hikvision digital technology co., ltd"), lit("hikvision"));
    m_shortVendorByName.insert(lit("arecont vision"), lit("arecontvision"));
}

QnResourceDataPool::~QnResourceDataPool() {
    return;
}

QnResourceData QnResourceDataPool::data(const QString &key) const
{
    QnMutexLocker lock(&m_mutex);
    return m_dataByKey.value(key.toLower());
}

QnResourceData QnResourceDataPool::data(const QnConstSecurityCamResourcePtr &camera) const {
    if (!camera)
        return QnResourceData();
    return data(camera->getVendor(), camera->getModel(), camera->getFirmware());
}

QnResourceData QnResourceDataPool::data(const QString& _vendor, const QString& _model, const QString& firmware) const {

    QString vendor = m_shortVendorByName.value(_vendor.toLower(), _vendor.toLower());
    QString model = _model.toLower();
    QStringList keyList;

    QString vendorAndModelKey = vendor + lit("|") + model;
    keyList.append(vendorAndModelKey);

    if (!firmware.isEmpty())
        keyList.append(vendorAndModelKey + lit("|") + firmware.toLower());

    QnResourceData result;
    QnMutexLocker lock(&m_mutex);
    for (const auto& key: keyList)
    {
        if (!m_cachedResultByKey.contains(key))
        {
            for (auto itr = m_dataByKey.begin(); itr != m_dataByKey.end(); ++itr)
            {
                if (wildcardMatch(itr.key(), key))
                    result.add(itr.value());
            }
            m_cachedResultByKey.insert(key, result);
        }
        else
        {
            result.add(m_cachedResultByKey[key]);
        }
    }
    return result;
}

bool QnResourceDataPool::loadFile(const QString &fileName)
{
    if(!QFile::exists(fileName))
    {
        qnWarning("File '%1' does not exist", fileName);
        return false;
    }

    if(!loadInternal(fileName))
    {
        qnWarning("Error while loading resource data from file '%1'.", fileName);
        return false;
    }

    return true;
}

bool QnResourceDataPool::loadInternal(const QString &fileName)
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
    bool result = false;

    if(!validateDataInternal(data, map, chunks))
        return false;

    {
        QnMutexLocker lock(&m_mutex);
        m_cachedResultByKey.clear();
        for(const QnResourceDataPoolChunk &chunk: chunks)
            for(const QString &key: chunk.keys)
                m_dataByKey[key.toLower()].add(chunk.data);
    }
    emit changed();
    return true;
}

void QnResourceDataPool::clear()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_cachedResultByKey.clear();
        m_dataByKey.clear();
    }
    emit changed();
}
