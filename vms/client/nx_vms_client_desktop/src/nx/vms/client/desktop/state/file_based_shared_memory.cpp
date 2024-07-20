// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_based_shared_memory.h"

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/branding.h>

#include "serialized_shared_memory_data.h"

#if defined(Q_OS_WIN)
    #include <io.h>
    #include <fileapi.h>
#else
    #include <fcntl.h>
#endif

namespace nx::vms::client::desktop {

namespace {

static const QString kDefaultSharedMemoryKey = nx::branding::customization()
    + "/DesktopClientFileBasedSharedMemory/"
    + QString::number(SerializedSharedMemoryData::kDataVersion);

static constexpr size_t kSharedDataSize = sizeof(SerializedSharedMemoryData);

} // namespace

struct FileBasedSharedMemory::Private
{
    Private(const QString& explicitId)
    {
        auto key = explicitId.isEmpty() ? kDefaultSharedMemoryKey : explicitId;
        key.replace(QRegularExpression("[/\\s:\\\\]"), "_");

        const QString location = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);

        sharedFile.setFileName(QDir(location).filePath(key));
    }

    bool initialize()
    {
        bool success = sharedFile.open(QIODeviceBase::ReadWrite | QIODeviceBase::ExistingOnly);

        if (!success && sharedFile.error() == QFileDevice::OpenError)
        {
            NX_DEBUG(this, "Creating shared memory file %1", sharedFile.fileName());
            success = sharedFile.open(QIODeviceBase::ReadWrite | QIODevice::NewOnly);
        }

        if (success)
        {
            bool initData = false;
            if (sharedFile.size() != kSharedDataSize)
                initData = NX_ASSERT(sharedFile.resize(kSharedDataSize));

            memory = sharedFile.map(0, kSharedDataSize);

            NX_ASSERT(memory, "Unable to map memory size %1", kSharedDataSize);

            if (initData)
            {
                lock();
                setData({});
                unlock();
            }
        }

        NX_ASSERT(success, "Could not acquire shared memory file %1", sharedFile.fileName());
        return success;
    }

    SharedMemoryData data() const
    {
        if (!NX_ASSERT(memory))
            return {};

        auto rawData = reinterpret_cast<SerializedSharedMemoryData*>(memory);
        return SerializedSharedMemoryData::deserializedData(*rawData);
    }

    void setData(const SharedMemoryData& data)
    {
        if (!NX_ASSERT(memory))
            return;

        auto rawData = reinterpret_cast<SerializedSharedMemoryData*>(memory);
        SerializedSharedMemoryData::serializeData(data, rawData);
    }

    void lock()
    {
        if (!NX_ASSERT(sharedFile.isOpen()))
            return;

        #if defined(Q_OS_WIN)
            OVERLAPPED overlapped = {};

            const BOOL success = LockFileEx(
                (HANDLE) _get_osfhandle(sharedFile.handle()),
                LOCKFILE_EXCLUSIVE_LOCK,
                /* dwReserved */ 0,
                /* nNumberOfBytesToLockLow */ kSharedDataSize,
                /* nNumberOfBytesToLockHigh */ 0,
                &overlapped);
        #else
            struct flock desc = {};
            desc.l_type = F_WRLCK;
            desc.l_whence = SEEK_SET;
            desc.l_start = 0;
            desc.l_len = kSharedDataSize;

            const bool success = (fcntl(sharedFile.handle(), F_SETLKW, &desc) == 0);
        #endif

        NX_ASSERT(success);
    }

    void unlock()
    {
        if (!NX_ASSERT(sharedFile.isOpen()))
            return;

        #if defined(Q_OS_WIN)
            OVERLAPPED overlapped = {};

            const BOOL success = UnlockFileEx(
                (HANDLE) _get_osfhandle(sharedFile.handle()),
                /* dwReserved */ 0,
                /* nNumberOfBytesToUnlockLow */ kSharedDataSize,
                /* nNumberOfBytesToUnlockHigh */ 0,
                &overlapped);
        #else
            struct flock desc = {};
            desc.l_type = F_UNLCK;
            desc.l_whence = SEEK_SET;
            desc.l_start = 0;
            desc.l_len = kSharedDataSize;

            const bool success = (fcntl(sharedFile.handle(), F_SETLKW, &desc) == 0);
        #endif

        NX_ASSERT(success);
    }

    uchar* memory = nullptr;
    QFile sharedFile;
};

FileBasedSharedMemory::FileBasedSharedMemory(const QString& explicitId):
    d(new Private(explicitId))
{
}

FileBasedSharedMemory::~FileBasedSharedMemory()
{
}

bool FileBasedSharedMemory::initialize()
{
    return d->initialize();
}

void FileBasedSharedMemory::lock()
{
    d->lock();
}

void FileBasedSharedMemory::unlock()
{
    d->unlock();
}

SharedMemoryData FileBasedSharedMemory::data() const
{
    return d->data();
}

void FileBasedSharedMemory::setData(const SharedMemoryData& data)
{
    d->setData(data);
}

} // namespace nx::vms::client::desktop
