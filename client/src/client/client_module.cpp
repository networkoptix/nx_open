#include "client_module.h"

#include <utils/common/module_resources.h>

#include <common/common_module.h>

#include "client_meta_types.h"
#include "client_settings.h"

QnClientModule::QnClientModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(client);

    QnClientMetaTypes::initialize();

    new QnCommonModule(argc, argv, this);
    new QnClientSettings(this);
}

QnClientModule::~QnClientModule() {
    return;
}

