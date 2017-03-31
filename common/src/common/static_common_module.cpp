#include "static_common_module.h"

#include <QtCore/QCoreApplication>

#include <nx/network/socket_global.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/common/long_runable_cleanup.h>
#include <api/http_client_pool.h>
#include <utils/common/synctime.h>

#include "common_meta_types.h"

QnStaticCommonModule::QnStaticCommonModule(
    Qn::PeerType localPeerType,
    const QString& brand,
    const QString& customization,
    QObject *parent)
    :
    QObject(parent),
    m_localPeerType(localPeerType),
    m_brand(brand),
    m_customization(customization)
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
    clear();
    nx::network::SocketGlobals::deinit();
}

Qn::PeerType QnStaticCommonModule::localPeerType() const
{
    return m_localPeerType;
}

QString QnStaticCommonModule::brand() const
{
    return m_brand;
}

QString QnStaticCommonModule::customization() const
{
    return m_customization;
}

QnResourceDataPool * QnStaticCommonModule::dataPool() const
{
    return m_dataPool;
}
