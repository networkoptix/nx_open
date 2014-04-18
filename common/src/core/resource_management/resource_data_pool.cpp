#include "resource_data_pool.h"

#include <QtCore/QFile>

#include <utils/common/warnings.h>
#include <utils/serialization/json.h>
#include <core/resource/camera_resource.h>

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
}

QnResourceDataPool::~QnResourceDataPool() {
    return;
}

QnResourceData QnResourceDataPool::data(const QString &key) const {
    return m_dataByKey.value(key.toLower());
}

QnResourceData QnResourceDataPool::data(const QnVirtualCameraResourcePtr &camera) const {
    QString vendor = camera->getVendor().toLower();
    vendor = m_shortVendorByName.value(vendor, vendor);
    QString model = camera->getModel().toLower();

    QString key0 = lit("*|") + model;
    QString key1 = vendor + lit("|") + model;
    QString key2 = key1 + lit("|") + camera->getFirmware().toLower();

    QnResourceData result;
    result.add(m_dataByKey.value(key0));
    result.add(m_dataByKey.value(key1));
    result.add(m_dataByKey.value(key2));
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

    foreach(const QnResourceDataPoolChunk &chunk, chunks)
        foreach(const QString &key, chunk.keys)
            m_dataByKey[key.toLower()].add(chunk.data);

    return true;
}
