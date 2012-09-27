#include "client_module.h"

#include <utils/common/module_resources.h>

#include <common_module.h>

QnClientModule::QnClientModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(client);

    new QnCommonModule(argc, argv, this);
}

QnClientModule::~QnClientModule() {
    return;
}

