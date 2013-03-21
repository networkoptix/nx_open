#include "client_module.h"

#include <utils/common/module_resources.h>

#include <common/common_module.h>

#include "client_meta_types.h"

QnClientModule::QnClientModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(client);

    QnClientMetaTypes::initialize();

    m_common = new QnCommonModule(argc, argv, this);
}

QnClientModule::~QnClientModule() {
    return;
}

