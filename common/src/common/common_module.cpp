#include "common_module.h"

#include <utils/common/module_resources.h>

#include <api/session_manager.h>

#include <common/common_meta_types.h>

#include "customization.h"

QnCommonModule::QnCommonModule(const QStringList& , QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(common);

    QnCommonMetaTypes::initilize();
    
    /* Init statics. */
    qnCustomization();
    qnProductFeatures();

    /* Init members. */
    m_sessionManager = new QnSessionManager(); //instance<QnSessionManager>();
}

QnCommonModule::~QnCommonModule() {
    delete m_sessionManager;
    return;
}

