#include "media_server_module.h"

#include <common/common_module.h>

QnMediaServerModule::QnMediaServerModule(QObject *parent):
    QObject(parent) 
{
    Q_INIT_RESOURCE(mediaserver_core);
    Q_INIT_RESOURCE(appserver2);

    m_common = new QnCommonModule(this);
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

