#include "common_module.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <common/common_meta_types.h>
#include <utils/common/module_resources.h>
#include <core/resource_managment/resource_data_pool.h>
#include <api/session_manager.h>

#include "customization.h"

QnCommonModule::QnCommonModule(int &, char **, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(common);

    QnCommonMetaTypes::initilize();
    
    /* Init statics. */
    qnProductFeatures();

    QnResourceDataPool *dataPool = instance<QnResourceDataPool>();
    loadResourceData(dataPool, QLatin1String(":/resource_data.json"));
    loadResourceData(dataPool, QCoreApplication::applicationDirPath() + QLatin1String("/resource_data.json"));

    /* Init members. */
    m_sessionManager = new QnSessionManager(); //instance<QnSessionManager>();
}

QnCommonModule::~QnCommonModule() {
    delete m_sessionManager;
    return;
}

void QnCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName) {
    if(QFile::exists(fileName))
        dataPool->load(fileName);
}
