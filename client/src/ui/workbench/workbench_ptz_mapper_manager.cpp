#include "workbench_ptz_mapper_manager.h"

#include <cassert>

#include <QtCore/QFile>
#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/settings.h>

#include <core/resource/camera_resource.h>


QnWorkbenchPtzMapperManager::QnWorkbenchPtzMapperManager(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    loadMappers();
}

QnWorkbenchPtzMapperManager::~QnWorkbenchPtzMapperManager() {
    qDeleteAll(m_mappers);
    m_mappers.clear();
    m_mapperByModel.clear();
}

QnPtzSpaceMapper *QnWorkbenchPtzMapperManager::mapper(const QString &model) const {
    return m_mapperByModel.value(model.toLower());
}

QnPtzSpaceMapper *QnWorkbenchPtzMapperManager::mapper(const QnVirtualCameraResourcePtr &resource) const {
    if(!resource) {
        qnNullWarning(resource);
        return NULL;
    }

    return mapper(resource->getModel());
}

const QList<QnPtzSpaceMapper *> &QnWorkbenchPtzMapperManager::mappers() const {
    return m_mappers;
}

void QnWorkbenchPtzMapperManager::addMapper(QnPtzSpaceMapper *mapper) {
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

void QnWorkbenchPtzMapperManager::removeMapper(QnPtzSpaceMapper *mapper) {
    if(!mapper) {
        qnNullWarning(mapper);
        return;
    }

    if(!m_mappers.removeOne(mapper))
        return; /* Removing mapper that is not there is OK. */

    foreach(const QString &model, mapper->models())
        m_mapperByModel.remove(model);
}

void QnWorkbenchPtzMapperManager::loadMappers() {
    QList<QString> mappingFileNames;
    mappingFileNames 
        << lit(":/ptz_mappings.json") 
        << QCoreApplication::applicationDirPath() + lit("/ptz_mappings.json")
        << qnSettings->extraPtzMappingsPath();

    foreach(const QString &fileName, mappingFileNames)
        if(QFile::exists(fileName))
            loadMappers(fileName);
}

bool QnWorkbenchPtzMapperManager::loadMappers(const QString &fileName) {
    bool result = loadMappersInternal(fileName);
    if(!result)
        qnWarning("Error while loading PTZ mappings from file '%1'.", fileName);
    return result;
}

bool QnWorkbenchPtzMapperManager::loadMappersInternal(const QString &fileName) {
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
