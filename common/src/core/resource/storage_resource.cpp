#include "storage_resource.h"
#include "utils/common/util.h"
#include "../dataprovider/media_streamdataprovider.h"


// ------------------------------ QnStorageResource -------------------------------

QnStorageResource::QnStorageResource()
    //m_writedSpace(0)
{
}

QnStorageResource::~QnStorageResource()
{
}


void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);
}

/*
qint64 QnStorageResource::getWritedSpace() const
{
    QMutexLocker lock(&m_writedSpaceMtx);
    return m_writedSpace;
}

void QnStorageResource::addWritedSpace(qint64 value)
{
    QMutexLocker lock(&m_writedSpaceMtx);
    m_writedSpace += value;
}
*/

// ---------------------------- QnStoragePluginFactory ------------------------------

Q_GLOBAL_STATIC(QnStoragePluginFactory, QnStoragePluginFactory_instance)

QnStoragePluginFactory::QnStoragePluginFactory(): m_defaultFactory(0)
{
}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

QnStoragePluginFactory *QnStoragePluginFactory::instance()
{
    return QnStoragePluginFactory_instance();
}

void QnStoragePluginFactory::registerStoragePlugin(const QString &protocol, const StorageResourceFactory &factory, bool isDefaultProtocol)
{
    m_factoryByProtocol.insert(protocol, factory);
    if (isDefaultProtocol)
        m_defaultFactory = factory;
}

QnStorageResource *QnStoragePluginFactory::createStorage(const QString &url, bool useDefaultForUnknownPrefix)
{
    int index = url.indexOf(lit("://"));
    if (index == -1) 
        return m_defaultFactory ? m_defaultFactory() : NULL;
    
    QString protocol = url.left(index);
    if (m_factoryByProtocol.contains(protocol)) {
        return m_factoryByProtocol.value(protocol)();
    } else {
        if (useDefaultForUnknownPrefix)
            return m_defaultFactory ? m_defaultFactory() : NULL;
        else
            return NULL;
    }
}
