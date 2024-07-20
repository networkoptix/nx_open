// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qt_based_shared_memory.h"

#include <QtCore/QDir>
#include <QtCore/QSharedMemory>

#include <nx/branding.h>
#include <nx/utils/log/assert.h>

#include "serialized_shared_memory_data.h"

namespace nx::vms::client::desktop {

namespace {

static const QString kDefaultSharedMemoryKey = nx::branding::customization()
    + "/DesktopClientQtBasedSharedMemory/"
    + QString::number(SerializedSharedMemoryData::kDataVersion) + "/"
    + QFileInfo(QDir::homePath()).owner();

} // namespace

struct QtBasedSharedMemory::Private
{
    Private(const QString& explicitId):
        memory(explicitId.isEmpty() ? kDefaultSharedMemoryKey : explicitId)
    {
    }

    bool initialize()
    {
        bool success = memory.create(sizeof(SerializedSharedMemoryData));
        if (success)
        {
            memory.lock();
            setData({});
            memory.unlock();
        }

        if (!success && memory.error() == QSharedMemory::AlreadyExists)
            success = memory.attach();

        NX_ASSERT(success, "Could not acquire shared memory");
        return success;
    }

    SharedMemoryData data() const
    {
        auto rawData = reinterpret_cast<SerializedSharedMemoryData*>(memory.data());
        return SerializedSharedMemoryData::deserializedData(*rawData);
    }

    void setData(const SharedMemoryData& data)
    {
        auto rawData = reinterpret_cast<SerializedSharedMemoryData*>(memory.data());
        SerializedSharedMemoryData::serializeData(data, rawData);
    }

    mutable QSharedMemory memory;
};

QtBasedSharedMemory::QtBasedSharedMemory(const QString& explicitId):
    d(new Private(explicitId))
{
}

QtBasedSharedMemory::~QtBasedSharedMemory()
{
}

bool QtBasedSharedMemory::initialize()
{
    return d->initialize();
}

void QtBasedSharedMemory::lock()
{
    d->memory.lock();
}

void QtBasedSharedMemory::unlock()
{
    d->memory.unlock();
}

SharedMemoryData QtBasedSharedMemory::data() const
{
    return d->data();
}

void QtBasedSharedMemory::setData(const SharedMemoryData& data)
{
    d->setData(data);
}

} // namespace nx::vms::client::desktop
