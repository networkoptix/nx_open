// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_plugin_factory.h"

#include <nx/utils/log/log.h>

nx::Lockable<QnStoragePluginFactory::StorageFactory> QnStoragePluginFactory::s_factory{};

QnStoragePluginFactory::QnStoragePluginFactory(QObject* parent):
    QObject(parent),
    m_defaultFactory()
{
}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

void QnStoragePluginFactory::registerStoragePlugin(
    const QString& protocol,
    const StorageFactory& factory,
    bool isDefaultProtocol)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_factoryByProtocol.insert(protocol, factory);
    if (isDefaultProtocol)
        m_defaultFactory = factory;
}

bool QnStoragePluginFactory::existsFactoryForProtocol(const QString& protocol)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_factoryByProtocol.find(protocol) == m_factoryByProtocol.end() ? false : true;
}

void QnStoragePluginFactory::setDefault(StorageFactory factory)
{
    *s_factory.lock() = factory;
}

QnStorageResource* QnStoragePluginFactory::createStorage(
    const QString& url,
    bool useDefaultForUnknownPrefix)
{
    if (url.isEmpty())
        return nullptr;

    if (const StorageFactory staticFactory = *s_factory.lock())
        return staticFactory(url);

    const int index = url.indexOf("://");
    StorageFactory factory;
    QString storageType;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (index == -1)
        {
            factory = m_defaultFactory;
        }
        else if (const auto it = m_factoryByProtocol.find(url.left(index));
            it != m_factoryByProtocol.end())
        {
            factory = it.value();
            storageType = it.key();
        }
        else if (useDefaultForUnknownPrefix)
        {
            factory = m_defaultFactory;
        }
    }

    if (!factory)
        return nullptr;

    QnStorageResource* const result = factory(url);
    if (storageType.isEmpty())
        return result;

    if (result == nullptr)
    {
        NX_ERROR(this, "Failed to create storage for url %1", url);
        return nullptr;
    }
    result->setStorageType(storageType);
    return result;
}
