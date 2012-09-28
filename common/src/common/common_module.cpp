#include "common_module.h"

#include <utils/common/module_resources.h>

QnCommonModule::QnCommonModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(common);
}

QnCommonModule::~QnCommonModule() {
    return;
}

