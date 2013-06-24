#include "common_module.h"

#include <utils/common/module_resources.h>

#include <api/session_manager.h>

#include <common/common_meta_types.h>

#include "customization.h"

QnCommonModule::QnCommonModule(int &, char **, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(common);

    QnCommonMetaTypes::initilize();
    
    /* Init statics. */
    qnCustomization();
    qnProductFeatures();

    /* Init members. */
    m_sessionManager = instance<QnSessionManager>();
}

QnCommonModule::~QnCommonModule() {
    return;
}

