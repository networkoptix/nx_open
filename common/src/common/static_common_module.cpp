#include "static_common_module.h"

#include <QtCore/QCoreApplication>

#include <nx/network/socket_global.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/common/long_runable_cleanup.h>
#include <api/http_client_pool.h>
#include <utils/common/synctime.h>

#include "common_meta_types.h"

QnStaticCommonModule::QnStaticCommonModule(QObject *parent):
    QObject(parent)
{
    Q_INIT_RESOURCE(common);
    QnCommonMetaTypes::initialize();
    nx::network::SocketGlobals::init();

    m_dataPool = instance<QnResourceDataPool>();
    loadResourceData(m_dataPool, lit(":/resource_data.json"), true);
    loadResourceData(m_dataPool, QCoreApplication::applicationDirPath() + lit("/resource_data.json"), false);

    store(new QnLongRunableCleanup());
    store(new nx::utils::TimerManager());

    instance<QnSyncTime>();
    instance<nx_http::ClientPool>();

}

void QnStaticCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);

    NX_ASSERT(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an NX_ASSERT here? Something is wrong with resource data json file. */
}

QnStaticCommonModule::~QnStaticCommonModule()
{
    nx::network::SocketGlobals::deinit();
}
