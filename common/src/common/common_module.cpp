#include "common_module.h"

#include <utils/common/module_resources.h>
#include <utils/common/warnings.h>
#include <utils/common/ptz_mapper_pool.h>

#include <api/session_manager.h>

#include <common/common_meta_types.h>

#include "customization.h"

QnCommonModule *QnCommonModule::s_instance = NULL;

QnCommonModule::QnCommonModule(int &, char **, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(common);

    QnCommonMetaTypes::initilize();
    
    /* Init statics. */
    qnCustomization();
    qnProductFeatures();

    /* Init members. */
    m_ptzMapperPool = new QnPtzMapperPool(this);
    m_sessionManager = new QnSessionManager();

    /* Init global instance. */
    if(s_instance) {
        qnWarning("QnCommonModule instance already exists.");
    } else {
        s_instance = this;
    }
}

QnCommonModule::~QnCommonModule() {
    if(s_instance == this)
        s_instance = NULL;

    delete m_sessionManager; // TODO: #Elric remove
}

