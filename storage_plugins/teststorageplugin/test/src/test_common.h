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
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461075"
                },
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461076"
                }
            ],
            "low": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461077"
                } 
            ]
        }
        ,
        {
            "id": "someCameraId2",
            "hi": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461078"
                }
            ],
            "low": [
                {
                    "durationMs": "429626247",
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

    virtual uint32_t readFileImpl(void* dst, uint32_t size, int* ecode) const override
    {
        if (m_name != "/path/to/sample/file.mkv")
            return 0;

        uint32_t bytesToRead = std::min((int)size, (int)strlen(kTestMediaFileContent) - (int)m_pos);
        memcpy(dst, kTestMediaFileContent + bytesToRead, bytesToRead);
        m_pos += bytesToRead;

        return bytesToRead;
    }

private:
    mutable uint32_t m_pos = 0;
};

auto ioDeviceDeleter = [](nx_spl::IODevice* ioDevice) 
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
        int size) const override
    {
        return new TestIODeviceHelper(name, (FileCategory)category, flags, size, nullptr);
    }
};


class TestStorageFactoryHelper : public TestStorageFactory
{
public:
    TestStorageFactoryHelper(bool invalidConfig = false) : m_invalidConfig(invalidConfig) {}
    std::string getConfigPath() const { return m_path; }

private:
    virtual bool readConfig(const std::string& path, std::string* outContent) override
    {
        if (m_invalidConfig)
            return false;

        m_path = path;
        outContent->assign(kTestJson);

        return true;
    }

    virtual nx_spl::Storage* createStorageImpl(const utils::VfsPair& vfsPair) override
    {
        return new TestStorageHelper(vfsPair);
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
