#include "media_server_module.h"

#include <QtCore/QCoreApplication>

#include <utils/common/module_resources.h>
#include <utils/common/ptz_mapper_pool.h>

#include <common/common_module.h>

QnMediaServerModule::QnMediaServerModule(int &argc, char **argv, QObject *parent):
    QObject(parent) 
{
    QN_INIT_MODULE_RESOURCES(mediaserver);

    QnCommonModule *common = new QnCommonModule(argc, argv, this);
    common->ptzMapperPool()->load(lit(":/ptz_mappers.json"));
    common->ptzMapperPool()->load(QCoreApplication::applicationDirPath() + lit("/ptz_mappers.json"));
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}
