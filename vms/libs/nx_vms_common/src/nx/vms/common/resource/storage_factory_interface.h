// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QnStoragePluginFactory;

namespace nx::vms::common {

/**
 * Interface for the Resource classes, which require access to the Storage factory.
 */
class StorageFactoryInterface {
public:
    StorageFactoryInterface(QnStoragePluginFactory* storageFactory):
        m_storageFactory(storageFactory)
    {
    }

    QnStoragePluginFactory* storageFactory() const
    {
        return m_storageFactory;
    }

private:
    QnStoragePluginFactory* const m_storageFactory;
};

} // namespace nx::vms::common
