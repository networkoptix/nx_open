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

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceDataPoolChunk *target) {
    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    QnResourceDataPoolChunk result;
    if(!QJson::deserialize(ctx, map, lit("keys"), &result.keys) || !QJson::deserialize(ctx, value, &result.data))
        return false;

    *target = result;
    return true;
}


QnResourceDataPool::QnResourceDataPool(QObject *parent): 
    QObject(parent) 
{
    m_shortVendorByName.insert(lit("digital watchdog"), lit("dw"));
    m_shortVendorByName.insert(lit("panoramic"), lit("dw"));
    m_shortVendorByName.insert(lit("ipnc"), lit("dw"));
    m_shortVendorByName.insert(lit("acti corporation"), lit("acti"));
    m_shortVendorByName.insert(lit("innovative security designs"), lit("isd"));
    m_shortVendorByName.insert(lit("norbain_"), lit("vista"));
    m_shortVendorByName.insert(lit("norbain"), lit("vista"));
    m_shortVendorByName.insert(lit("flir systems"), lit("flir"));
    m_shortVendorByName.insert(lit("hanwha techwin"), lit("hanwha"));
}

QnResourceDataPool::~QnResourceDataPool() {
    return;
}

QnResourceData QnResourceDataPool::data(const QString &key) const {
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
    
    {
        QnMutexLocker lock(&m_cachedDataMtx);

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
    }

    return result;    
}

bool QnResourceDataPool::load(const QString &fileName) {
    if(!QFile::exists(fileName)) {
        qnWarning("File '%1' does not exist", fileName);
        return false;
    }

    if(!loadInternal(fileName)) {
        qnWarning("Error while loading resource data from file '%1'.", fileName);
        return false;
    }

    return true;
}

bool QnResourceDataPool::loadInternal(const QString &fileName) {
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 size = file.size();
    if(size == 0 || size > 16 * 1024 * 1024)
        return false; /* File is larger than 16Mb => definitely not JSON we need. */

    QByteArray data = file.readAll();
    if(data.isEmpty())
        return false; /* Read error. */
    file.close();

    QJsonObject map;
    if(!QJson::deserialize(data, &map))
        return false;

    QString version;
    if(!QJson::deserialize(map, lit("version"), &version) || version != lit("1.0"))
        return false;

    QList<QnResourceDataPoolChunk> chunks;
    if(!QJson::deserialize(map, lit("data"), &chunks))
        return false;

    for(const QnResourceDataPoolChunk &chunk: chunks)
        for(const QString &key: chunk.keys)
            m_dataByKey[key.toLower()].add(chunk.data);

    return true;
}
