#include "ptz_mapper_pool.h"

#include <QtCore/QFile>

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/math/space_mapper.h>


QnPtzMapperPool::QnPtzMapperPool(QObject *parent):
    QObject(parent) 
{}

QnPtzMapperPool::~QnPtzMapperPool() {
    return;
}

QnPtzMapperPtr QnPtzMapperPool::mapper(const QString &model) const {
    return m_mapperByModel.value(model.toLower());    
}

bool QnPtzMapperPool::load(const QString &fileName) {
    if(!QFile::exists(fileName)) {
        qnWarning("File '%1' does not exist", fileName);
        return false;
    }

    if(!loadInternal(fileName)) {
        qnWarning("Error while loading PTZ mappings from file '%1'.", fileName);
        return false;
    }
        
    return true;
}

bool QnPtzMapperPool::loadInternal(const QString &fileName) {
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 size = file.size();
    if(size == 0 || size > 16 * 1024 * 1024)
        return false; /* File is larger than 16Mb => definitely not JSON we need. */

    QByteArray data = file.readAll();
    if(data.isEmpty())
        return false; /* Read error. */

    QVariantMap map;
    if(!QJson::deserialize(data, &map))
        return false;

    QString version = map.value(lit("version")).toString();
    /* +------------------+-------------------------+
     * | Software version | Ptz mapper file version |
     * +------------------+-------------------------+
     * | <= 2.0           | 1.0                     |
     * | 2.1              | 1.1                     |
     * +------------------+-------------------------+ */
    if(version != lit("1.1"))
        return false;

    QList<QnPtzSpaceMapper> mappers;
    if(!QJson::deserialize(map.value(lit("mappers")), &mappers))
        return false;

    foreach(const QnPtzSpaceMapper &mapper, mappers)
        addMapper(new QnPtzSpaceMapper(mapper));
    return true;
}

