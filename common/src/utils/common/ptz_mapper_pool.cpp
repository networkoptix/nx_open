#include "ptz_mapper_pool.h"

#include <QtCore/QFile>

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/space_mapper.h>


QnPtzMapperPool::QnPtzMapperPool(QObject *parent):
    QObject(parent) 
{}

QnPtzMapperPool::~QnPtzMapperPool() {
    qDeleteAll(m_mappers);
    m_mappers.clear();
    m_mapperByModel.clear();
}

const QnPtzSpaceMapper *QnPtzMapperPool::mapper(const QString &model) const {
    return m_mapperByModel.value(model.toLower());    
}

const QnPtzSpaceMapper *QnPtzMapperPool::mapper(const QnVirtualCameraResourcePtr &resource) const {
    if(!resource) {
        qnNullWarning(resource);
        return NULL;
    }

    return mapper(resource->getModel());
}

void QnPtzMapperPool::addMapper(const QnPtzSpaceMapper *mapper) {
    if(!mapper) {
        qnNullWarning(mapper);
        return;
    }

    if(m_mappers.contains(mapper)) {
        qnWarning("Given mapper is already registered with this mapping manager.");
        return;
    }

    m_mappers.push_back(mapper);
    foreach(const QString &model, mapper->models())
        m_mapperByModel[model.toLower()] = mapper;
}

void QnPtzMapperPool::removeMapper(const QnPtzSpaceMapper *mapper) {
    if(!mapper) {
        qnNullWarning(mapper);
        return;
    }

    if(!m_mappers.removeOne(mapper))
        return; /* Removing mapper that is not there is OK. */

    foreach(const QString &model, mapper->models())
        m_mapperByModel.remove(model);
}

bool QnPtzMapperPool::load(const QString &fileName) {
    if(!QFile::exists(fileName)) {
        qnWarning("File '%1' does not exist", fileName);
        return false;
    }

    if(!loadMappersInternal(fileName)) {
        qnWarning("Error while loading PTZ mappings from file '%1'.", fileName);
        return false;
    }
        
    return true;
}

bool QnWorkbenchPtzMapperManager::loadInternal(const QString &fileName) {
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
    if(version != lit("1.0"))
        return false;

    QList<QnPtzSpaceMapper> mappers;
    if(!QJson::deserialize(map.value(lit("mappers")), &mappers))
        return false;

    foreach(const QnPtzSpaceMapper &mapper, mappers)
        addMapper(new QnPtzSpaceMapper(mapper));
    return true;
}

