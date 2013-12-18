#include "media_server_module.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <utils/common/module_resources.h>

#include <common/common_module.h>

QnMediaServerModule::QnMediaServerModule(int &argc, char **argv, QObject *parent):
    QObject(parent) 
{
    QN_INIT_MODULE_RESOURCES(mediaserver);

    m_common = new QnCommonModule(argc, argv, this);
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

