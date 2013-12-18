#include "media_server_module.h"

#include <common/common_module.h>

#include <core/ptz/server_ptz_controller_pool.h>

QnMediaServerModule::QnMediaServerModule(int &argc, char **argv, QObject *parent):
    QObject(parent) 
{
    Q_INIT_RESOURCE(mediaserver);

    m_common = new QnCommonModule(argc, argv, this);
    
    m_common->instance<QnServerPtzControllerPool>();
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

