#include "common_module.h"

#include <utils/common/module_resources.h>

#include "customization.h"

QnCommonModule::QnCommonModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    
    QN_INIT_MODULE_RESOURCES(common);
    
    /* Init statics. */
    qnCustomization();
    qnProductFeatures();
}

QnCommonModule::~QnCommonModule() {
    return;
}

