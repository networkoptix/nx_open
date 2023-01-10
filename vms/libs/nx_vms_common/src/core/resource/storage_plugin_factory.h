// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/utils/thread/mutex.h>

#include "storage_resource.h"

class NX_VMS_COMMON_API QnStoragePluginFactory: public QObject
{
public:
    typedef std::function<QnStorageResource *(const QString&)> StorageFactory;

    QnStoragePluginFactory(QObject* parent = nullptr);
    virtual ~QnStoragePluginFactory();

    bool existsFactoryForProtocol(const QString &protocol);
    void registerStoragePlugin(
        const QString &protocol,
        const StorageFactory &factory,
        bool isDefaultProtocol = false);

    QnStorageResource *createStorage(
        const QString &url,
        bool useDefaultForUnknownPrefix = true);

    static void setDefault(StorageFactory factory);

private:
    QHash<QString, StorageFactory> m_factoryByProtocol;
    StorageFactory m_defaultFactory;
    static StorageFactory s_factory;
    nx::Mutex m_mutex; // TODO: #rvasilenko this mutex is not used, is it intentional?
};
