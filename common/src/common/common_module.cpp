#include "common_module.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <common/common_meta_types.h>
#include <utils/common/product_features.h>
#include <core/resource_management/resource_data_pool.h>
#include <api/session_manager.h>


QnCommonModule::QnCommonModule(int &, char **, QObject *parent): QObject(parent) {
    Q_INIT_RESOURCE(common);
    m_cloudMode = false;

    QnCommonMetaTypes::initialize();
    
    /* Init statics. */
    qnProductFeatures();

    m_dataPool = instance<QnResourceDataPool>();
    loadResourceData(m_dataPool, lit(":/resource_data.json"));
    loadResourceData(m_dataPool, QCoreApplication::applicationDirPath() + lit("/resource_data.json"));

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
