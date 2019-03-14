#pragma once

#include <memory>
#include <gtest/gtest.h>
#include <test_storage_factory.h>
#include <test_storage.h>
#include <test_io_device.h>

    /**
    1453550461075 - Sat Jan 23 2016 15:01:01
    */
    const char* const kTestJson = R"JSON(
{
    "sample": "/path/to/sample/file.mkv",
    "cameras": [
        {
            "id": "someCameraId1",
            "hi": [
                {
                    "durationMs": "60000",
                    "startTimeMs": "1453550461075"
                },
                {
                    "durationMs": "60000",
                    "startTimeMs": "1453550461076"
                }
            ],
            "low": [
                {
                    "durationMs": "60000",
                    "startTimeMs": "1453550461077"
                }
            ]
        }
        ,
        {
            "id": "someCameraId2",
            "hi": [
                {
                    "durationMs": "60000",
                    "startTimeMs": "1453550461078"
                }
            ],
            "low": [
                {
                    "durationMs": "60000",
                    "startTimeMs": "1453550461079"
                }
            ]
        }
    ]
}
)JSON";

const char* const kTestMediaFileContent = "1234567890";


class TestIODeviceHelper : public TestIODevice
{
public:
    using TestIODevice::TestIODevice;

    virtual uint32_t readImpl(void* dst, uint32_t size, int* ecode) const override
    {
        if (m_sampleFileName != "/path/to/sample/file.mkv")
            return 0;

        uint32_t bytesToRead = std::min((int)size, (int)strlen(kTestMediaFileContent) - (int)m_pos);
        memcpy(dst, kTestMediaFileContent + m_pos, bytesToRead);
        m_pos += bytesToRead;

        return bytesToRead;
    }

    virtual int seekImpl(uint64_t pos, int* ecode)
    {
        if (pos < 0ULL || pos >= strlen(kTestMediaFileContent))
        {
            if (ecode)
                *ecode = nx_spl::error::UnknownError;
            return 0;
        }
        m_pos = pos;

        return 1;
    }

private:
    mutable uint32_t m_pos = 0;
};

const auto ioDeviceDeleter = [](nx_spl::IODevice* ioDevice)
{
    if (ioDevice)
        ioDevice->releaseRef();
};
using IODeviceUniquePtr = std::unique_ptr<TestIODeviceHelper, decltype(ioDeviceDeleter)>;


class TestStorageHelper : public TestStorage
{
public:
    using TestStorage::TestStorage;

    virtual nx_spl::IODevice* createIODevice(
        const std::string& name,
        int category,
        int flags,
        int64_t size,
        int* ecode) const override
    {
        if (ecode)
            *ecode = nx_spl::error::NoError;

        return new TestIODeviceHelper(name, (FileCategory)category, flags, size, nullptr);
    }
};


class TestStorageFactoryHelper : public TestStorageFactory
{
public:
    TestStorageFactoryHelper(bool invalidConfig = false) : m_invalidConfig(invalidConfig) {}

    std::string getConfigPath() const { return m_path; }
    const std::unordered_set<std::string>& storageHosts() const { return m_storageHosts; }

private:
    virtual bool readConfig(const std::string& path, std::string* outContent) override
    {
        if (m_invalidConfig)
            return false;

        m_path = path;
        outContent->assign(kTestJson);

        return true;
    }

    virtual nx_spl::Storage* createStorageImpl(const utils::VfsPair& vfsPair, const utils::Url& url) override
    {
        return new TestStorageHelper(vfsPair,
                                     url.scheme() + "://" + url.host(),
                                     [this, url]()
                                     {
                                        std::lock_guard<std::mutex> lock(m_storageHostsMutex);
                                        m_storageHosts.erase(url.host());
                                     });
    }

    std::string m_path;
    bool m_invalidConfig;
};

class TestStorageTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        storage = factory.createStorage("test://storage/some/path", nullptr);
        ASSERT_TRUE(storage);
    }

    virtual void TearDown() override
    {
        storage->releaseRef();
    }

    TestStorageFactoryHelper factory;
    nx_spl::Storage* storage;
};
