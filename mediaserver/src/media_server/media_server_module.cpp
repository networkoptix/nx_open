#include "media_server_module.h"

#include <common/common_module.h>

QnMediaServerModule::QnMediaServerModule(int &argc, char **argv, QObject *parent):
    QObject(parent) 
{
    Q_INIT_RESOURCE(mediaserver);
    Q_INIT_RESOURCE(appserver2);

    m_common = new QnCommonModule(argc, argv, this);
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

