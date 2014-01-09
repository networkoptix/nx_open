#include "resource_data_pool.h"

#include <QtCore/QFile>

#include <utils/common/warnings.h>
#include <utils/common/json.h>
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
{}

QnResourceDataPool::~QnResourceDataPool() {
    return;
}

QnResourceData QnResourceDataPool::data(const QString &key) const {
    QMutexLocker locker(&m_mutex);

    return m_dataByKey.value(key.toLower());
}

QnResourceData QnResourceDataPool::data(const QnVirtualCameraResourcePtr &camera) const {
    /* No need to lock here. */
    return data(/*camera->getVendorName() + lit('.') +*/ camera->getModel()); // TODO: #Elric use vendor here!
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

    QMutexLocker locker(&m_mutex);
    foreach(const QnResourceDataPoolChunk &chunk, chunks)
        foreach(const QString &key, chunk.keys)
            m_dataByKey[key.toLower()].addData(chunk.data);

    return true;
}
