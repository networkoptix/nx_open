#include "media_server_module.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <utils/common/module_resources.h>
#include <utils/common/ptz_mapper_pool.h>

#include <common/common_module.h>

QnMediaServerModule::QnMediaServerModule(const QStringList& args, QObject *parent):
    QObject(parent) 
{
    QN_INIT_MODULE_RESOURCES(mediaserver);

    m_common = new QnCommonModule(args, this);

    QnPtzMapperPool *ptzMapperPool = m_common->instance<QnPtzMapperPool>();
    loadPtzMappers(ptzMapperPool, QLatin1String(":/ptz_mappers.json"));
    loadPtzMappers(ptzMapperPool, QCoreApplication::applicationDirPath() + QLatin1String("/ptz_mappers.json"));
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

void QnMediaServerModule::loadPtzMappers(QnPtzMapperPool *ptzMapperPool, const QString &fileName) {
    if(QFile::exists(fileName))
        ptzMapperPool->load(fileName);
}

