#include "media_server_module.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <utils/common/module_resources.h>
#include <utils/common/ptz_mapper_pool.h>

#include <common/common_module.h>

QnMediaServerModule::QnMediaServerModule(int &argc, char **argv, QObject *parent):
    QObject(parent) 
{
    QN_INIT_MODULE_RESOURCES(mediaserver);

    m_common = new QnCommonModule(argc, argv, this);

    loadPtzMappers(lit(":/ptz_mappers.json"));
    loadPtzMappers(QCoreApplication::applicationDirPath() + lit("/ptz_mappers.json"));
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

void QnMediaServerModule::loadPtzMappers(const QString &fileName) {
    if(QFile::exists(fileName))
        m_common->ptzMapperPool()->load(fileName);
}

